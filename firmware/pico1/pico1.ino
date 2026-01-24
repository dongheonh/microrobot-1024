#include <Arduino.h>
#include <Wire.h>
#include "command.h"

static uint8_t seq_received[4];
static uint8_t data256[UART_PAYLOAD_BYTES]; // 256
static uint8_t X[X_VALUES];

constexpr uint32_t UART_BAUD = 921600;
constexpr uint32_t I2C_HZ    = 1000000;

void setup() {
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(UART_BAUD);

  Wire.begin();
  Wire1.begin();
  Wire.setClock(I2C_HZ);
  Wire1.setClock(I2C_HZ);
}

void loop() {
  // 1) Read SEQ(4)
  readExactBytes(Serial1, seq_received, 4);
  uint32_t seq = rd_u32_le(seq_received);

  // 2) Read 256 bytes payload
  readExactBytes(Serial1, data256, UART_PAYLOAD_BYTES);

  // 3) build X from these 256 bytes and do local I2C actions
  buildX(data256, X);
  actionX(Wire, Wire1, X);

  // 4) ACK back to Pico2 send seq (u-turn of seq)
  uint8_t ack7[ACK_BYTES];
  makeAck(ack7, seq, /*status=*/1);
  Serial1.write(ack7, ACK_BYTES);
  Serial1.flush();
}
