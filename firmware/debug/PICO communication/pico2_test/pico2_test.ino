#include <Arduino.h>
#include <Wire.h>
#include "command.h"

static uint8_t hdr[HDR_BYTES];          // 6
static uint8_t data512[DATA_BYTES];     // 512 pure
static uint8_t crc2[CRC_BYTES];         // 2
static uint8_t X512[X_VALUES];          // 512 values for local i2c action

constexpr uint32_t UART_BAUD = 921600;
constexpr uint32_t I2C_HZ    = 1000000;
constexpr uint32_t ACK_TIMEOUT_US = 200000; // 200ms

void setup() {
  Serial.begin(0);

  Serial1.setTX(0);  // GP0
  Serial1.setRX(1);  // GP1
  Serial1.begin(UART_BAUD);

  Wire.begin();
  Wire1.begin();
  Wire.setClock(I2C_HZ);
  Wire1.setClock(I2C_HZ);
}

void loop() {
  // 1) Read frame header (MAGIC+SEQ)
  readExactBytes(Serial, hdr, HDR_BYTES);
  if (rd_u16_le(&hdr[0]) != MAGIC) return;
  uint32_t seq = rd_u32_le(&hdr[2]);

  // 2) Read 512 data
  readExactBytes(Serial, data512, DATA_BYTES);

  // 3) Read CRC16 and verify
  readExactBytes(Serial, crc2, CRC_BYTES);
  uint16_t crc_rx = rd_u16_le(crc2);
  uint16_t crc_ok = (crc16_ccitt(data512, DATA_BYTES) == crc_rx);

  if (!crc_ok) {
    // CRC fail -> do not forward; PC will timeout and resend
    return;
  }

  // 4) Pico2 -> Pico1: send SEQ(4) + first 256 data
  uint8_t seq4[4];
  wr_u32_le(seq4, seq);
  writeExactBytes(Serial1, seq4, 4);
  writeExactBytes(Serial1, data512, DATA_HALF);
  Serial1.flush();

  // 5) Pico2 local I2C actions using second half (256 bytes)
  buildX(data512 + DATA_HALF, X512); // unpack 256 -> 512 values
  actionX(Wire, Wire1, X512);

  // 6) Wait Pico1 ACK then forward ACK to PC
  uint8_t status = 0;
  bool ack_ok = readAck(Serial1, seq, &status, ACK_TIMEOUT_US);
  if (ack_ok) {
    uint8_t ack7[ACK_BYTES];
    makeAck(ack7, seq, status);
    Serial.write(ack7, ACK_BYTES); // PC waits for this before next frame
  } else {
    // no ack -> PC will timeout and resend
  }
}
