#include "command.h"
#include <string.h>

// Author: DH HAN and SAM LAB

// ++++ READ and WRITE ++++ 
// READ from stream "s" | save at "dst" | size: "n" bytes
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

// from "src (init addr)" | size: "n bytes" | WRITE to stream "s"
void writeExactBytes(Stream& s, const uint8_t* src, int n) {
  int sent = 0;
  while (sent < n) {
    int w = s.write(src + sent, n - sent);
    sent += w;
  }
}

// ++++ BYTES UTIL ++++

// ---- A. READ ----
// 2 bytes READ buffer | from the addr p get 2 bytes) | pc -> pico2
uint16_t rd_u16_le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);      // ** left shift data by 8 bits (1byte)
}

// 4 bytes READ buffer | from the addr p get 32 bits (4 bytes) | pc -> pico2 && pico2 -> pico1 
uint32_t rd_u32_le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}           // ** left shift data by 8 bits (1byte)


// ---- B. WRITE ---- 
// 2 bytes write buffer | write  
void wr_u16_le(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);                   // ** right shift data by 8 bits (1 byte)
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

// 4 bytes write buffer:  write sequence from pico2 -> pico1
void wr_u32_le(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);                   // ** right shift data by 8 bits (1 byte)
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

// ++++ CRC ALGORITHM ++++
// CRC create - host side
static inline uint16_t crc16_update(uint16_t crc, uint8_t data) {   
  crc ^= ((uint16_t)data << 8);         // ** shift data 
  for (int i = 0; i < 8; i++) {
    crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

// CRC validate - slave side
uint16_t crc16_ccitt(const uint8_t* data, int n, uint16_t init) {
  uint16_t crc = init;
  for (int i = 0; i < n; i++) crc = crc16_update(crc, data[i]);
  return crc;
}

// ++++ BUILD CONTROL INPUT ++++ 
void buildX(const uint8_t* packed256, uint8_t* X512) {
  for (int i = 0; i < 256; i++) {
    uint8_t b = packed256[i];
    X512[2*i + 0] = (uint8_t)(b & 0x0F);
    X512[2*i + 1] = (uint8_t)((b >> 4) & 0x0F);         // ** shift data by 4 bits (0.5 byte)
  }
}

// ++++ ACTION (send final signal via I2C) ++++
// IMPROTANT: Communication rule: 
// -> for ALL buses (i2c0, i2c1) start from 0x40 address increase number by 1    
// -> PCA9685 has 16 output, 8 couples
// -> each couple has [data1, data2]

// for each DATA from X512 has the range of {0, ... ,15} (2bit), each data tells you intensity of a magnet
//  -> intensity = value - 7
//  -> example: if value == 5, then intensity = -2
//  -> example: if value == 7, then intensity = 0
//  -> 15: is never be used

// for one I2C address, for 8 couples of data | i = 0, i++, 7 
// -> if the intensity < 0
//     convert it to pwm intensity, then return [pwmintensity, 0]
// -> if the intensity > 0, 
//     convert it to pwm intensity, then return [0, pwmintensity]
// -> if the intensity == 0, 
//     convert it to pwm intensity, then return [0,0]
// -> make pwmintensity array  
// send pwmintensity array (16 pwm values => data) to a i2c address  
// repeat 32 times and increment i2c address 

// ACTION_X
// - Two I2C buses: bus0 and bus1
// - Each bus controls 32 PCA9685 boards at addresses 0x40..0x5F
// - Each PCA9685 controls 8 magnets via 16 PWM channels (pair per magnet):
//     pairIdx m -> (left=2m, right=2m+1)
//     intensity > 0  => LEFT = pwm, RIGHT = 0
//     intensity < 0  => LEFT = 0,   RIGHT = pwm
//     intensity = 0  => both OFF
//
// Coding rule (X512):
//   value in {0..14} only (15 is forbidden)
//   intensity = value - 7   => range [-7..+7]
//   pwm magnitude is based on |intensity| mapped to 0..4095

static constexpr uint8_t  BASE_ADDR  = 0x40;
static constexpr int      NUM_BOARDS = 32;
static constexpr int      MAG_PER_BRD = 8;
static constexpr uint16_t PWM_MAX    = 4095;

// output magnitude of pwm (0..4095) from intensity magnitude (|intensity|)
static inline uint16_t intensityToPwm(int intensity) {
  if (intensity == 0) return 0;
  int mag = abs(intensity);                 // 1..7
  // linear map: mag=7 -> 4095, mag=1 -> 585 (approx)
  return (uint16_t)((mag * PWM_MAX) / 7);
}

// "intensity" is to check the polarity | "pairIdx" is the index you save | "Adafruit_PWMServoDriver" object from the library
// for each driver, update pwm ("setPWM") to the obj, obj addr
static inline void setPair(Adafruit_PWMServoDriver& b, int pairIdx, int intensity, uint16_t pwm) {
  const int left  = 2 * pairIdx;
  const int right = left + 1;

  // controlling polarity by selecting which side of the pair is driven (H-bridge direction)
  if (intensity > 0) {
    b.setPWM(left,  0, pwm);
    b.setPWM(right, 0, 0);
  } else if (intensity < 0) {
    b.setPWM(left,  0, 0);
    b.setPWM(right, 0, pwm);
  } else {
    b.setPWM(left,  0, 0);
    b.setPWM(right, 0, 0);
  }
}

// Action Main function | apply (applyBus) X512 (magnet state of 512 magnets - 256 bytes) to both buses (128+ byte/bus).
// - boards0[i] corresponds to address BASE_ADDR + i on bus0
// - boards1[i] corresponds to address BASE_ADDR + i on bus1
// Adafruit_PWMServoDriver: class type 
void actionX(Adafruit_PWMServoDriver* boards0[NUM_BOARDS], Adafruit_PWMServoDriver* boards1[NUM_BOARDS], const uint8_t* X512) {

  // "boards[NUM_BOARDS]" is pca9685 obj | "Xbase" is 256byte (512 magnet state) | for 32 boards, considering each board board[i]
  auto applyBus = [&](Adafruit_PWMServoDriver* boards[NUM_BOARDS], const uint8_t* Xbase) {
    
    // for loop takes a PCA9685 as a chunck
    for (int dev = 0; dev < NUM_BOARDS; ++dev) {                          // boards0(32) or boards1(32) set
      Adafruit_PWMServoDriver* b = boards[dev];                           // save a board object | e.g. boards[0] = new Adafruit_PWMServoDriver(0x40) (== b);
      if (!b) continue;                                                   // if board object is not assigned skip a loop

      // for each PCA9685's addr, 8 magnets per board -> 8 pairs -> 16 PWM channels
      for (int m = 0; m < MAG_PER_BRD; ++m) {                             // global constexpr MAG_Per_BRD = 8
        const uint8_t value = Xbase[dev * MAG_PER_BRD + m];               // get an intensity value from X 

        // value 15 is forbidden; safest behavior: turn this magnet OFF
        if (value == 15) {
          setPair(*b, m, 0, 0);
          continue;
        }

        const int intensity = (int)value - 7;                 // [-7..+7] subtract 7 (offset), make first discrete intensity
        const uint16_t pwm  = intensityToPwm(intensity);      // trasnslate to PWM value for PCA9685 to output

        setPair(*b, m, intensity, pwm);                       // make a motor driver input signal, keep it into the Adafruit_PWMServoDriver* boards0, boards1
      }
    }
  };
  
  // bus0 boards: X512[0..255] (256 magnets = 32 boards * 8 magnets)
  applyBus(boards0, X512);

  // bus1 boards: X512[256..511]
  applyBus(boards1, X512 + 256);
}

// ++++ ACK (verification of successful communication) ++++ 
// Ack has 7 bytes magic(0x55AA) + seq + status(T or F)

// Make Ack from "pico1" and move it to PC | pico1 -> pico2 -> PC
// Pico1: use "seq" from stream "s" from Pico2 
void makeAck(uint8_t* ack7, uint32_t seq, uint8_t status) {
  wr_u16_le(&ack7[0], ACK_MAGIC);
  wr_u32_le(&ack7[2], seq);
  ack7[6] = status;
}

// pico2 read Ack and write
// Read from stream "s" | expected_seq: this # has to be same (from Pico1 and PC) | 
bool readAck(Stream& s, uint32_t expected_seq, uint8_t* out_status, uint32_t timeout_us) {
  uint8_t buf[ACK_BYTES];                                   // constant (fixed size buffer = 7 bytes)
  int idx = 0;
  uint32_t t0 = micros();                                   // pico2's timestamp

  while ((micros() - t0) < timeout_us) {                    // read data in timeout_us - reading 7 bytes
    while (s.available() && idx < ACK_BYTES) {              // read and save data in buff 
      buf[idx++] = (uint8_t)s.read();
    }
    if (idx < ACK_BYTES) continue;                          // when 7 bytes are not recieved                  

    if (rd_u16_le(&buf[0]) == ACK_MAGIC && rd_u32_le(&buf[2]) == expected_seq) {  // checking if magic and seq are correct if so.. 
      if (out_status) *out_status = buf[6];                                       // if correct, then update out status (1 byte) -> success or not
      return true;                                                                // also return true as a function output and end the function 
    }

    // resync shift 1 byte
    memmove(buf, buf + 1, ACK_BYTES - 1);
    idx = ACK_BYTES - 1;
  }
  return false;                                                                   // return false to show readAck failed = communication failed
}