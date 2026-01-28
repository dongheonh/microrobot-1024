// ===========================================
// filename: pico1.ino
// ===========================================
#include "command.h"
#include <Adafruit_PWMServoDriver.h>

// Author: DH HAN and SAM LAB
//
// IMPORTANT (do not break comment intent)
// - Pico1 receives UART packet from Pico2:
//     [SEQ(4)] + [DATA_HALF(256 bytes)]
// - Pico1 unpacks the 256 bytes into X512 (512 values 0..15)
// - Pico1 applies actionX() to its two I2C buses (64 boards total -> 512 magnets)
// - Pico1 returns ACK(7) to Pico2:
//     [ACK_MAGIC(2)] + [SEQ(4)] + [STATUS(1)]
//
// PCA9685 addressing rule (per bus):
// - start BASE_ADDR=0x40, increment by 1
// - 32 boards per bus => 0x40..0x5F


// ++++ CONFIG (EDIT ONLY THESE) ++++
static constexpr uint8_t  BASE_ADDR = 0x40;
static constexpr uint32_t I2C_HZ    = 1000000;
static constexpr float    PCA_PWM_FREQ_HZ = 1000.0f;

// status codes (keep consistent with your system)
static constexpr uint8_t STATUS_OK = 1;


// ++++ GLOBAL BUFFERS ++++
static uint8_t seq4[UART_SEQ_BYTES];          // 4 bytes
static uint8_t packed256[UART_PAYLOAD_BYTES]; // 256 bytes
static uint8_t X[X_VALUES];                   // 512 values (0..15)
static uint8_t ack7[ACK_BYTES];

// ++++ PCA9685 OBJECTS ++++
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
  // UART link from Pico2
  Serial1.begin(115200);

  // I2C buses on Pico1
  Wire.begin();
  Wire1.begin();
  Wire.setClock(I2C_HZ);
  Wire1.setClock(I2C_HZ);

  // PCA bring-up
  initPcaBus(boards0, Wire,  BASE_ADDR);
  initPcaBus(boards1, Wire1, BASE_ADDR);
}


// ++++ MAIN LOOP ++++
void loop() {

  // ============================================
  // 1) Receive UART packet: SEQ(4) + DATA(256)
  // ============================================
  readExactBytes(Serial1, seq4, UART_SEQ_BYTES);
  readExactBytes(Serial1, packed256, UART_PAYLOAD_BYTES);

  const uint32_t seq = rd_u32_le(&seq4[0]);

  // ============================================
  // 2) Unpack and apply on Pico1
  // ============================================
  buildX(packed256, X);
  actionX(boards0, boards1, X);

  // ============================================
  // 3) Send ACK back to Pico2
  // ============================================
  makeAck(ack7, seq, STATUS_OK);
  writeExactBytes(Serial1, ack7, ACK_BYTES);
}
