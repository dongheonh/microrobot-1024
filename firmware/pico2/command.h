#pragma once
#include <Arduino.h>
#include <Wire.h>

// system sizes (1024 electromagnet array)
constexpr int NUM_CHANNELS  = 1024;                       // controlling 1024 electromagnets
constexpr int PACKED_BYTES = NUM_CHANNELS / 2;            // using 512 bytes

// sizes for pico2
constexpr int NUM_CHANNELS_PICO = 512;                    // 512 X on Pico2
constexpr int PACKED_BYTES_HALF = PACKED_BYTES_TOTAL/2;   //


constexpr uint8_t PCA_BASE_ADDR = 0x40;   // the PCA9685's address A5..A0 = 000000
constexpr int NUM_PCA = 32;               // total # of PCA9685 
constexpr int X_PER_PCA = 8;              // 8 electromagnets per PCA9685
constexpr int ACTION_X_LEN = NUM_PCA * X_PER_PCA;  // 256 Xi




// Build X from packed 512 bytes
void buildX(const uint8_t* packed_in, uint8_t* X_out, int pakced_bytes);

// Blocking read of one frame (512 bytes)
bool readExactBytes(Stream& s, uint8_t* dst, int n);

#endif
