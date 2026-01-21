#include "command.h"

uint8_t packed[PACKED_BYTES];    // USB data - 512 byte
uint8_t X[NUM_CHANNELS];        // unpacked 4-bit intensities, intensity level Xi is an element of {0,1,...,15}

// pico2.ino

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);

  while (!Serial) {}
  
  // debug -m
  Serial.println("setup complete");

}

void loop() {
  // read from the PC
  readExactBytes(Serial, packed, PACKED_BYTES);   // reading 512 bytes
  
  // send to the pico1
  Serial1.write(packed, PACKED_BYTES / 2);        // sending 256 bytes
  Serial1.flush();
  
  // before action - pico2 action uses 2nd half (packed + (PACKED_BYTES / 2)) 
  buildX(packed + (PACKED_BYTES / 2), X, PACKED_BYTES / 2);            // 256 bytes for action on pico2

  // action
  actionX(Wire, X, ACTION_X_LEN);

}
