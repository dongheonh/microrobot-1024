#include <Arduino.h>
#include <Wire.h>
#include "command.h"

static uint8_t hdr[HDR_BYTES];          // 6 frame header (MAGIC+SEQ)
static uint8_t data512[DATA_BYTES];     // 512 pure
static uint8_t crc[CRC_BYTES];          // crc 2
static uint8_t X[X_VALUES];             // 512 values for local i2c action

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
  readExactBytes(Serial, hdr, HDR_BYTES);         // first read header -> read exact bytes: 6 bytes and save as hdr

  // from frame header split -> seq and magic
  if (rd_u16_le(&hdr[0]) != MAGIC) return;        // read value (from the address: &hdr[0] -> hdr[1]) and make 16-bit value (hdr[{0,1}]), compare with MAGIC
  uint32_t seq_read = rd_u32_le(&hdr[2]);              // split seq

  // 2) Read 512 data
  readExactBytes(Serial, data512, DATA_BYTES);    // data512 includes pico1, pico2 action data 

  // 3) Read CRC16 and verify
  readExactBytes(Serial, crc, CRC_BYTES);
  uint16_t crc_rx = rd_u16_le(crc);
  uint16_t crc_ok = (crc16_ccitt(data512, DATA_BYTES) == crc_rx);   // read command.h, if crc is correct, proceed 

  if (!crc_ok) {
    // CRC fail --> PC will count this 
    return;
  }

  // 4) Pico2 -> Pico1: send SEQ(4) + first 256 data
  uint8_t seq[4];
  wr_u32_le(seq, seq_read);
  writeExactBytes(Serial1, seq, 4);
  writeExactBytes(Serial1, data512, DATA_HALF);
  Serial1.flush();

  // 5) Pico2 local I2C actions using second half (256 bytes)
  buildX(data512 + DATA_HALF, X); // unpack 256 -> 512 values
  actionX(Wire, Wire1, X);

  // 6) Wait Pico1 ACK then forward ACK to PC
  uint8_t status = 0;
  bool ack_ok = readAck(Serial1, seq, &status, ACK_TIMEOUT_US); // validating BUF crosschecking
  if (ack_ok) {
    uint8_t ack7[ACK_BYTES];
    makeAck(ack7, seq, status);
    Serial.write(ack7, ACK_BYTES); // PC waits for this before next frame (same as pico->pico2 what recieved)
  } else {
    // no ack -> PC will timeout and resend
  }
}
