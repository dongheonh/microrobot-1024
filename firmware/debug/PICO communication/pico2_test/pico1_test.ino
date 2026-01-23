#include <Arduino.h>
#include <Wire.h>
#include "command.h"

static uint8_t packed[PACKED_BYTES];   // 512 bytes from PC
static uint8_t X[X_VALUES_PICO];       // 512 values (0..15)

void setup() {
  // USB
  Serial.begin(0);

  // UART to pico1 (pins fixed)
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(921600);

  // I2C0/I2C1
  Wire.begin();
  Wire1.begin();
  Wire.setClock(1000000);
  Wire1.setClock(1000000);

  initPcaAll(Wire, Wire1);
}

void loop() {
  // A) USB: read 512 bytes
  readExactBytes(Serial, packed, PACKED_BYTES);

  // B) forward first half to pico1 over UART (256 bytes)
  Serial1.write(packed, PACKED_HALF);
  Serial1.flush();

  // C) unpack second half (256 bytes) -> X[0..511]
  buildX(packed + PACKED_HALF, X, PACKED_HALF);

  // D) action on pico2: X[0..255] -> I2C0, X[256..511] -> I2C1
  actionX(Wire, Wire1, X);
}
