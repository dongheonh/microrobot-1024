#pragma once
#include <Arduino.h>
#include <Wire.h>

// ===== Data sizes =====
constexpr int DATA_BYTES   = 512;          // pure magnet data: 512 bytes (1024 magnets * 4 bits)
constexpr int DATA_HALF    = DATA_BYTES/2; // 256 bytes
constexpr int X_VALUES     = DATA_HALF*2;  // 512 nibbles -> 512 values (0..15)

// ===== Framing (PC <-> Pico2) =====
constexpr uint16_t MAGIC = 0x55AA;         // bytes: AA 55
constexpr int HDR_BYTES  = 6;              // MAGIC(2)+SEQ(4)
constexpr int CRC_BYTES  = 2;              // CRC16
constexpr int FRAME_BYTES = HDR_BYTES + DATA_BYTES + CRC_BYTES; // 520 bytes total

// ===== UART (Pico2 <-> Pico1) =====
// Pico2 sends: SEQ(4) + 256 data bytes
constexpr int UART_SEQ_BYTES = 4;
constexpr int UART_PAYLOAD_BYTES = 256;

// Pico1 ACK back: ACK_MAGIC(2)+SEQ(4)+STATUS(1)
constexpr uint16_t ACK_MAGIC = 0x33CC;     // bytes: CC 33
constexpr int ACK_BYTES = 7;

// ===== API =====
void readExactBytes(Stream& s, uint8_t* dst, int n);
void writeExactBytes(Stream& s, const uint8_t* src, int n);

uint16_t rd_u16_le(const uint8_t* p);
uint32_t rd_u32_le(const uint8_t* p);
void     wr_u16_le(uint8_t* p, uint16_t v);
void     wr_u32_le(uint8_t* p, uint32_t v);

// CRC16-CCITT (0x1021), init 0xFFFF
uint16_t crc16_ccitt(const uint8_t* data, int n, uint16_t init=0xFFFF);

// unpack 256 bytes -> 512 values (0..15)
void buildX(const uint8_t* packed256, uint8_t* X512);

// action: X[0..255] -> I2C0, X[256..511] -> I2C1
void actionX(TwoWire& i2c0, TwoWire& i2c1, const uint8_t* X512);

// ACK helpers
void makeAck(uint8_t* ack7, uint32_t seq, uint8_t status);
bool readAck(Stream& s, uint32_t expected_seq, uint8_t* out_status, uint32_t timeout_us);
