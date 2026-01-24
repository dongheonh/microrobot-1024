#include "command.h"

void readExactBytes(Stream& s, uint8_t* dst, int n) {
  int got = 0;
  while (got < n) {
    int avail = s.available();
    if (avail <= 0) continue;

    int take = avail;
    if (take > (n - got)) take = (n - got);

    int r = s.readBytes((char*)(dst + got), take);
    got += r;
  }
}

void writeExactBytes(Stream& s, const uint8_t* src, int n) {
  int sent = 0;
  while (sent < n) {
    int w = s.write(src + sent, n - sent);
    sent += w;
  }
}

uint16_t rd_u16_le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
uint32_t rd_u32_le(const uint8_t* p) {
  return (uint32_t)p[0]
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}
void wr_u16_le(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}
void wr_u32_le(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static inline uint16_t crc16_update(uint16_t crc, uint8_t data) {
  crc ^= ((uint16_t)data << 8);
  for (int i = 0; i < 8; i++) {
    crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

uint16_t crc16_ccitt(const uint8_t* data, int n, uint16_t init) {
  uint16_t crc = init;
  for (int i = 0; i < n; i++) crc = crc16_update(crc, data[i]);
  return crc;
}

void buildX(const uint8_t* packed256, uint8_t* X512) {
  for (int i = 0; i < 256; i++) {
    uint8_t b = packed256[i];
    X512[2*i + 0] = (uint8_t)(b & 0x0F);
    X512[2*i + 1] = (uint8_t)((b >> 4) & 0x0F);
  }
}

// -------- actionX --------
static constexpr uint8_t I2C_ADDR0 = 0x40; // TODO: set
static constexpr uint8_t I2C_ADDR1 = 0x40; // TODO: set
static constexpr int I2C_CHUNK = 32;

static int i2c_send(TwoWire& W, uint8_t addr, const uint8_t* data, int n) {
  W.beginTransmission(addr);
  W.write(data, n);
  return W.endTransmission(); // 0=OK
}

void actionX(TwoWire& i2c0, TwoWire& i2c1, const uint8_t* X512) {
  // i2c0: X[0..255] (256 bytes)
  for (int off = 0; off < 256; off += I2C_CHUNK) {
    i2c_send(i2c0, I2C_ADDR0, X512 + off, I2C_CHUNK);
  }
  // i2c1: X[256..511] (256 bytes)
  for (int off = 0; off < 256; off += I2C_CHUNK) {
    i2c_send(i2c1, I2C_ADDR1, X512 + 256 + off, I2C_CHUNK);
  }
}

void makeAck(uint8_t* ack7, uint32_t seq, uint8_t status) {
  wr_u16_le(&ack7[0], ACK_MAGIC);
  wr_u32_le(&ack7[2], seq);
  ack7[6] = status;
}

bool readAck(Stream& s, uint32_t expected_seq, uint8_t* out_status, uint32_t timeout_us) {
  uint8_t buf[ACK_BYTES];
  int idx = 0;
  uint32_t t0 = micros();

  while ((micros() - t0) < timeout_us) {
    while (s.available() && idx < ACK_BYTES) {
      buf[idx++] = (uint8_t)s.read();
    }
    if (idx < ACK_BYTES) continue;

    if (rd_u16_le(&buf[0]) == ACK_MAGIC && rd_u32_le(&buf[2]) == expected_seq) {
      if (out_status) *out_status = buf[6];
      return true;
    }

    // resync shift 1 byte
    memmove(buf, buf + 1, ACK_BYTES - 1);
    idx = ACK_BYTES - 1;
  }
  return false;
}
