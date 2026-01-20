#ifndef COMMAND_H
#define COMMAND_H

#include <Arduino.h>

constexpr int NUM_CHANNELS  = 1024;   // controlling 1024 electromagnets
constexpr int PACKED_BYTES = 512;     // using 512 bytes

// Build X from packed 512 bytes
void buildX(const uint8_t* packed_in, uint8_t* X_out);

// Blocking read of one frame (512 bytes)
bool readExactBytes(Stream& s, uint8_t* dst, int n);

#endif
