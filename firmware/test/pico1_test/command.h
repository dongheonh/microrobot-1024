// ===========================================
// filename: command.h
// ===========================================
#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Author: DH HAN and SAM LAB

// ++++ COMMUNICATION ++++
//
// Frame formats
// (A) PC <-> Pico2 (USB Serial)
//   [HDR: MAGIC(2) + SEQ(4)] + [DATA: 512 bytes] + [CRC16: 2 bytes]  => total 520 bytes
//
// (B) Pico2 <-> Pico1 (UART)
//   Pico2 sends: SEQ(4) + PAYLOAD(256 bytes)  (and Pico1 returns ACK)
//
// ACK format (Pico1 -> Pico2 -> PC)
//   ACK_BYTES = 7 bytes
//   [ACK_MAGIC(2)] + [SEQ(4)] + [STATUS(1)]
//
// NOTE
// - All multi-byte fields here are LITTLE-ENDIAN (LE).

// ++++ PROTOCOL CONSTANTS ++++
static constexpr uint16_t MAGIC     = 0x55AA;   // bytes on wire: AA 55 (LE)
static constexpr uint16_t ACK_MAGIC = 0x55AA;   // ACK magic (same value, but kept explicit for clarity)

static constexpr int HDR_BYTES   = 6;           // MAGIC(2) + SEQ(4)
static constexpr int CRC_BYTES   = 2;           // CRC16-CCITT
static constexpr int ACK_BYTES   = 7;           // ACK_MAGIC(2) + SEQ(4) + STATUS(1)

// ++++ DATA SIZES ++++
//
// 1024 magnets are represented as 4-bit values (0..15) packed into 512 bytes.
// Pico2 splits that into two halves (256 bytes each) for local buses / forwarding.
static constexpr int DATA_BYTES   = 512;        // packed magnet data for 1024 magnets
static constexpr int DATA_HALF    = DATA_BYTES / 2; // 256 bytes
static constexpr int X_VALUES     = DATA_HALF * 2;  // 512 nibbles -> 512 values (0..15)

// Full frame size PC <-> Pico2
static constexpr int FRAME_BYTES  = HDR_BYTES + DATA_BYTES + CRC_BYTES; // 520 bytes total

// UART payload size Pico2 -> Pico1
static constexpr int UART_SEQ_BYTES      = 4;
static constexpr int UART_PAYLOAD_BYTES  = 256;

// ++++ BYTES UTIL ++++
//
// READ little-endian integers from a byte buffer (pc -> pico2, pico2 -> pico1)
// WRITE little-endian integers into a byte buffer (pico2 -> pico1, pico1 -> pc)
uint16_t rd_u16_le(const uint8_t* p);                 // 2 bytes read (LE)
uint32_t rd_u32_le(const uint8_t* p);                 // 4 bytes read (LE)
void     wr_u16_le(uint8_t* p, uint16_t v);           // 2 bytes write (LE)
void     wr_u32_le(uint8_t* p, uint32_t v);           // 4 bytes write (LE)

// ++++ STREAM IO ++++
//
// Exact read/write helpers for Stream (Serial / UART).
// - readExactBytes: blocks until exactly n bytes are read into dst
// - writeExactBytes: blocks until exactly n bytes are written from src
void readExactBytes(Stream& s, uint8_t* dst, int n);
void writeExactBytes(Stream& s, const uint8_t* src, int n);

// ++++ CRC ALGORITHM ++++
//
// CRC16-CCITT for validating frames (host computes CRC, slave validates).
// init default is 0xFFFF (common CRC-CCITT init).
uint16_t crc16_ccitt(const uint8_t* data, int n, uint16_t init = 0xFFFF);

// ++++ BUILD CONTROL INPUT ++++
//
// buildX:
// - Input: packed256 (256 bytes) = 512 magnets * 4 bits
// - Output: X512 (512 values) each in {0..15}
//   X512[2*i+0] = low nibble
//   X512[2*i+1] = high nibble
void buildX(const uint8_t* packed256, uint8_t* X512);

// ++++ ACTION (send final signal via I2C) ++++
//
// actionX signature MUST match command.cpp:
// - boards0 and boards1 are arrays of pointers to Adafruit_PWMServoDriver objects.
// - X512 is 512 magnet states:
//     X512[0..255]   -> bus0 boards (32 boards * 8 magnets)
//     X512[256..511] -> bus1 boards (32 boards * 8 magnets)
//
// NOTE
// - The actual I2C writes are performed via Adafruit_PWMServoDriver::setPWM inside command.cpp.
// - Any internal helper (intensityToPwm, setPair, etc.) stays in command.cpp to avoid duplication.
void actionX(Adafruit_PWMServoDriver* boards0,
             Adafruit_PWMServoDriver* boards1,
             const uint8_t* X512);

// ++++ ACK (verification of successful communication) ++++
//
// makeAck:
// - Builds 7-byte ACK: [ACK_MAGIC(2)] + [SEQ(4)] + [STATUS(1)]
// - Used by Pico1 to respond upward (Pico1 -> Pico2 -> PC)
void makeAck(uint8_t* ack7, uint32_t seq, uint8_t status);

// readAck:
// - Reads ACK_BYTES (=7) from stream with timeout
// - Verifies ACK_MAGIC and expected_seq
// - If valid: writes status to out_status and returns true
// - If timeout or mismatch: returns false
bool readAck(Stream& s, uint32_t expected_seq, uint8_t* out_status, uint32_t timeout_us);
