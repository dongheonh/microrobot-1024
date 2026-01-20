#include "command.h"

uint8_t packed[PACKED_BYTES];    // USB data
uint8_t X[NUM_CHANNELS];        // unpacked 4-bit intensities

// pico2.ino

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  
  // debug -m
  Serial.println("setup complete");


}



void loop() {
  readExactBytes(Serial, packed, PACKED_BYTES);
  buildX(packed, X)


}
