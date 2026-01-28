// ===========================================
// filename: pico2.ino
// ===========================================
#include "command.h"
#include <Adafruit_PWMServoDriver.h>

// Author: DH HAN and SAM LAB
//
// IMPORTANT (do not break comment intent)
// - PC -> Pico2 frame format (exact):
//     HDR_BYTES (6)  : MAGIC(2) + SEQ(4)
//     DATA_BYTES(512): packed 4-bit magnet values for 1024 magnets
//     CRC_BYTES (2)  : CRC16-CCITT over [HDR + DATA] (little-endian stored)
// - Pico2 splits DATA into two halves:
//     first 256 bytes  -> forwarded to Pico1 over UART (with SEQ)
//     second 256 bytes -> used locally on Pico2 (buildX + actionX)
// - Pico2 waits ACK from Pico1 (ACK_BYTES=7) and then sends ACK to PC
// - PCA9685 addressing rule (per bus):
//     start BASE_ADDR=0x40, increment by 1
//     32 boards per bus => 0x40..0x5F
//
// NOTE
// - All validation must be strict:
//     MAGIC must match
//     CRC must match
//     ACK must match expected SEQ
// - If CRC fails, do NOT forward to Pico1; just return status fail to PC.


// ++++ CONFIG (EDIT ONLY THESE) ++++
static constexpr uint8_t  BASE_ADDR = 0x40;
static constexpr uint32_t I2C_HZ    = 1000000;
static constexpr float    PCA_PWM_FREQ_HZ = 1000.0f;

// ACK status codes (1 byte)
// - keep it simple and explicit
static constexpr uint8_t STATUS_OK            = 1;
static constexpr uint8_t STATUS_ERR_MAGIC     = 0;
static constexpr uint8_t STATUS_ERR_CRC       = 2;
static constexpr uint8_t STATUS_ERR_PICO1_ACK = 3;


// ++++ GLOBAL BUFFERS ++++
// frame parts
static uint8_t hdr[HDR_BYTES];              // MAGIC(2) + SEQ(4)
static uint8_t data512[DATA_BYTES];         // packed 512 bytes (1024 magnets * 4 bits)
static uint8_t crc2[CRC_BYTES];             // received CRC (2 bytes)

// computed check buffer (HDR + DATA)
static uint8_t checkBuf[HDR_BYTES + DATA_BYTES];

// Pico2 local action buffer
static uint8_t X[X_VALUES];                 // 512 values (0..15) for 512 magnets controlled by Pico2

// ack buffers
static uint8_t ack7[ACK_BYTES];             // Pico2 -> PC ACK
static uint8_t pico1_status = 0;


// ++++ PCA9685 OBJECTS ++++
// Two buses on Pico2 (Wire, Wire1), each has 32 boards.
static Adafruit_PWMServoDriver* boards0[32];
static Adafruit_PWMServoDriver* boards1[32];

static void initPcaBus(Adafruit_PWMServoDriver* boards[32], TwoWire& wire, uint8_t base_addr) {
  for (int i = 0; i < 32; ++i) {
    const uint8_t addr = (uint8_t)(base_addr + i);
    boards[i] = new Adafruit_PWMServoDriver(addr, &wire);
    boards[i]->begin();
    boards[i]->setPWMFreq(PCA_PWM_FREQ_HZ);
    delay(2);
  }
}


// ++++ SETUP ++++
void setup() {
  // ---- A. SERIAL ----
  Serial.begin(115200);     // PC <-> Pico2 (USB)
  Serial1.begin(115200);    // Pico2 <-> Pico1 (UART)
  while (!Serial) {}

  // ---- B. I2C ----
  Wire.begin();
  Wire1.begin();
  Wire.setClock(I2C_HZ);
  Wire1.setClock(I2C_HZ);

  // ---- C. PCA bring-up ----
  initPcaBus(boards0, Wire,  BASE_ADDR);   // bus0: 0x40..0x5F
  initPcaBus(boards1, Wire1, BASE_ADDR);   // bus1: 0x40..0x5F

  Serial.println("pico2 setup complete");
}


// ++++ MAIN LOOP ++++
void loop() {

  // ============================================
  // 1) Read frame header: MAGIC(2) + SEQ(4)
  // ============================================
  readExactBytes(Serial, hdr, HDR_BYTES);

  // verify MAGIC first (strict)
  const uint16_t magic = rd_u16_le(&hdr[0]);
  const uint32_t seq   = rd_u32_le(&hdr[2]);

  if (magic != MAGIC) {
    // consume the rest of the frame defensively (to resync)
    // but note: if stream is misaligned, this may still be noisy.
    readExactBytes(Serial, data512, DATA_BYTES);
    readExactBytes(Serial, crc2, CRC_BYTES);

    makeAck(ack7, seq, STATUS_ERR_MAGIC);
    writeExactBytes(Serial, ack7, ACK_BYTES);
    return;
  }

  // ============================================
  // 2) Read DATA(512) and CRC(2)
  // ============================================
  readExactBytes(Serial, data512, DATA_BYTES);
  readExactBytes(Serial, crc2, CRC_BYTES);

  // ============================================
  // 3) CRC validate over [HDR + DATA]
  // ============================================
  // build check buffer = hdr + data
  memcpy(checkBuf, hdr, HDR_BYTES);
  memcpy(checkBuf + HDR_BYTES, data512, DATA_BYTES);

  const uint16_t crc_recv = rd_u16_le(&crc2[0]);
  const uint16_t crc_calc = crc16_ccitt(checkBuf, (HDR_BYTES + DATA_BYTES), 0xFFFF);

  if (crc_recv != crc_calc) {
    makeAck(ack7, seq, STATUS_ERR_CRC);
    writeExactBytes(Serial, ack7, ACK_BYTES);
    return;
  }

  // ============================================
  // 4) Forward FIRST HALF (256 bytes) to Pico1 with SEQ
  // ============================================
  // UART payload rule:
  // - Pico2 -> Pico1: [SEQ(4)] + [256 bytes]
  uint8_t uart_pkt[UART_SEQ_BYTES + UART_PAYLOAD_BYTES];
  wr_u32_le(&uart_pkt[0], seq);
  memcpy(&uart_pkt[UART_SEQ_BYTES], data512, UART_PAYLOAD_BYTES);

  writeExactBytes(Serial1, uart_pkt, (UART_SEQ_BYTES + UART_PAYLOAD_BYTES));

  // ============================================
  // 5) Local action on Pico2 using SECOND HALF (256 bytes)
  // ============================================
  // data512[256..511] => unpack to X[0..511] (0..15)
  buildX(data512 + DATA_HALF, X);

  // apply to two buses (Pico2 controls 512 magnets)
  actionX(boards0, boards1, X);

  // ============================================
  // 6) Read ACK from Pico1 (must match expected SEQ)
  // ============================================
  // timeout_us must be chosen realistically for:
  // - Pico1 UART receive + I2C apply + ACK send-back
  // start with 200ms and tune down later
  const uint32_t ACK_TIMEOUT_US = 200000;

  bool ok = readAck(Serial1, seq, &pico1_status, ACK_TIMEOUT_US);
  if (!ok) {
    makeAck(ack7, seq, STATUS_ERR_PICO1_ACK);
    writeExactBytes(Serial, ack7, ACK_BYTES);
    return;
  }

  // ============================================
  // 7) Send final ACK to PC
  // ============================================
  // If Pico1 reports failure (status byte), propagate it as-is (or map if you want).
  // Here: if pico1_status == 1 => OK, else => use that status directly.
  const uint8_t final_status = (pico1_status == 1) ? STATUS_OK : pico1_status;

  makeAck(ack7, seq, final_status);
  writeExactBytes(Serial, ack7, ACK_BYTES);
}
