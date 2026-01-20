#include "command.h"

// Build X from packed 512 bytes (PACKED_BYTES)
void buildX(const uint8_t* packed_in, uint8_t* X_out) {
  for (int i = 0; i < PACKED_BYTES; ++i) {
    uint8_t b = packed_in[i];
    X_out[2 * i]     = b & 0x0F;          // low nibble: first 4 bit
    X_out[2 * i + 1] = (b >> 4) & 0x0F;   // push 4 bit right (b >> 4), therefore high nibble: last 4 bit 
  }
}

// Blocking read of one frame (512 bytes) - read from USB
bool readExactBytes(Stream& s, uint8_t* dst, int n) {
  int received = 0;
  while (received < n) {
    if (s.available()) {
      dst[received++] = s.read();
    }
  }
  return true;
}
