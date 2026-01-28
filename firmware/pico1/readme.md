{\rtf1\ansi\ansicpg1252\cocoartf2867
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fmodern\fcharset0 Courier;}
{\colortbl;\red255\green255\blue255;\red0\green0\blue0;}
{\*\expandedcolortbl;;\cssrgb\c0\c0\c0;}
\margl1440\margr1440\vieww11520\viewh8400\viewkind0
\deftab720
\pard\pardeftab720\partightenfactor0

\f0\fs26 \cf0 \expnd0\expndtw0\kerning0
\outl0\strokewidth0 \strokec2 # 1024-channel electromagnet streaming protocol (PC \uc0\u8596  Pico2 \u8596  Pico1)\
\
Author: DH HAN and SAM LAB\
\
This repository implements a strict, frame-based streaming protocol to control a **1024-channel electromagnet array** in real time.\
\
System overview:\
- **PC** sends one control frame per update.\
- **Pico2** validates the frame (MAGIC/SEQ/CRC), then:\
  - forwards the **first half** of magnet data to **Pico1** over UART (with SEQ),\
  - applies the **second half** locally to its own PCA9685 banks over I2C,\
  - waits for **Pico1 ACK**, then returns a final **ACK** back to the PC.\
- **Pico1** receives the forwarded payload, applies it to its PCA9685 banks, and returns an ACK.\
\
---\
\
## directory (expected)\
\
- `command.h`  \
- `command.cpp`  \
- `pico2.ino`  \
- `pico1.ino`  \
- `pc_host.py`  \
\
---\
\
## protocol (rigorous)\
\
All multi-byte fields are **little-endian**.\
\
### 1) PC \uc0\u8594  Pico2 (USB Serial) : TX frame\
\
Total size: **520 bytes**\
\
| field | bytes | description |\
|------|------:|-------------|\
| MAGIC | 2 | constant `0x55AA` (LE on wire) |\
| SEQ | 4 | frame sequence number (uint32 LE) |\
| DATA | 512 | packed 4-bit values for 1024 magnets |\
| CRC16 | 2 | CRC16-CCITT(init=0xFFFF) over **[HDR + DATA]** |\
\
Where:\
- `HDR_BYTES = 6 = MAGIC(2) + SEQ(4)`\
- `DATA_BYTES = 512`\
- `CRC_BYTES = 2`\
- `FRAME_BYTES = 520`\
\
#### DATA packing rule (host side)\
\
Each magnet intensity is a **4-bit value** in `\{0,1,...,15\}`:\
- `15` is forbidden by design (firmware turns that magnet OFF as a safety rule)\
- recommended active range is `0..14`\
\
Packing 1024 values into 512 bytes:\
- `packed[i] = (hi << 4) | lo`\
- `lo = values[2*i + 0] & 0x0F`\
- `hi = values[2*i + 1] & 0x0F`\
\
This is the exact inverse of `buildX()` on MCU:\
- `X[2*i+0] = packed[i] & 0x0F`\
- `X[2*i+1] = (packed[i] >> 4) & 0x0F`\
\
---\
\
### 2) Pico2 \uc0\u8594  Pico1 (UART) : forwarded packet\
\
Total size: **260 bytes**\
\
| field | bytes | description |\
|------|------:|-------------|\
| SEQ | 4 | same SEQ as PC frame (uint32 LE) |\
| PAYLOAD | 256 | first half of DATA (bytes `data512[0..255]`) |\
\
Where:\
- `UART_SEQ_BYTES = 4`\
- `UART_PAYLOAD_BYTES = 256`\
\
---\
\
### 3) Pico1 \uc0\u8594  Pico2 \u8594  PC : ACK\
\
Total size: **7 bytes**\
\
| field | bytes | description |\
|------|------:|-------------|\
| ACK_MAGIC | 2 | constant `0x55AA` (LE on wire) |\
| SEQ | 4 | must match the TX frame SEQ |\
| STATUS | 1 | success/failure code |\
\
Where:\
- `ACK_BYTES = 7`\
\
Status conventions:\
- `1` : OK\
- other values: error codes (system-defined)\
  - Pico2 may use explicit codes like CRC fail, Pico1 timeout, etc.\
\
---\
\
## magnet actuation rule (PCA9685)\
\
### topology\
- Two I2C buses per controller:\
  - bus0 (`Wire`) controls 32 PCA boards\
  - bus1 (`Wire1`) controls 32 PCA boards\
- Each PCA9685 board has 16 PWM channels = 8 pairs\
- Each pair drives one magnet using polarity routing (H-bridge direction)\
\
### addressing rule\
On each I2C bus:\
- base address `0x40`\
- board `i` uses `0x40 + i`\
- 32 boards \uc0\u8594  `0x40..0x5F`\
\
### X512 to PWM rule\
For each magnet:\
- `value` in `\{0..14\}` only (`15` forbidden)\
- `intensity = value - 7` \uc0\u8594  range `[-7..+7]`\
- PWM magnitude uses `|intensity|` mapped linearly to `[0..4095]`\
\
Polarity routing (per magnet pair):\
- `intensity > 0`  \uc0\u8594  LEFT = pwm, RIGHT = 0\
- `intensity < 0`  \uc0\u8594  LEFT = 0,   RIGHT = pwm\
- `intensity = 0`  \uc0\u8594  both OFF\
\
---\
\
## files and responsibilities\
\
### `command.h / command.cpp`\
Shared protocol + utilities:\
- exact stream read/write (`readExactBytes`, `writeExactBytes`)\
- endian helpers (`rd_u16_le`, `rd_u32_le`, `wr_u16_le`, `wr_u32_le`)\
- CRC16-CCITT (`crc16_ccitt`)\
- unpacking (`buildX`)\
- actuation (`actionX`)\
- ACK helpers (`makeAck`, `readAck`)\
\
IMPORTANT:\
- `command.h` must match `command.cpp` signatures strictly.\
- do not duplicate implementation in headers (avoid ODR/duplicate symbol issues).\
\
### `pico2.ino`\
- reads full TX frame from PC\
- validates MAGIC and CRC\
- forwards first 256 bytes to Pico1 (with SEQ)\
- applies second 256 bytes locally (buildX + actionX)\
- waits for Pico1 ACK\
- returns final ACK to PC\
\
### `pico1.ino`\
- receives forwarded UART packet (SEQ + 256 bytes)\
- unpacks (buildX) and applies (actionX)\
- returns ACK to Pico2\
\
### `pc_host.py`\
- constructs TX frame (MAGIC + SEQ + DATA + CRC)\
- sends to Pico2\
- reads ACK and validates (magic + seq)\
- example pattern generator included\
\
---\
\
## bring-up checklist\
\
1) Wire both I2C buses correctly (Wire, Wire1) and confirm pull-ups.\
2) Confirm PCA9685 address straps match the expected range:\
   - 0x40..0x5F on each bus (32 boards)\
3) Start Pico1 and Pico2 firmware.\
4) On PC, run `pc_host.py` and confirm:\
   - ACKs are received\
   - SEQ increments correctly\
   - status returns `1`\
\
---\
\
## tuning notes\
\
- If ACK timeouts occur:\
  - increase Pico2 `ACK_TIMEOUT_US`\
  - lower I2C clock from 1 MHz to 400 kHz for stability\
- If bus reliability is poor:\
  - add small delays between PCA updates\
  - confirm SDA/SCL wiring and grounding across all boards\
\
---\
}