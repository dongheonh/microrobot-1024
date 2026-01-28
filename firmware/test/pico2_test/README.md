# 1024-channel electromagnet streaming protocol (PC ↔ Pico2 ↔ Pico1)

Author: DH HAN and SAM LAB

This repository implements a **strict, frame-based streaming protocol** to control a **1024‑channel electromagnet array** in real time.  
All protocol rules, data sizes, and behaviors are consistent with `command.h` / `command.cpp`.

---

## system overview

- **PC**
  - Builds and sends framed control packets (MAGIC + SEQ + DATA + CRC).
  - Receives final ACK and validates sequence and status.

- **Pico2**
  - Validates MAGIC and CRC.
  - Splits DATA:
    - first half → forwarded to Pico1 over UART (with SEQ),
    - second half → applied locally over I2C.
  - Waits for Pico1 ACK, then sends final ACK to PC.

- **Pico1**
  - Receives forwarded UART payload.
  - Applies magnet commands over I2C.
  - Returns ACK to Pico2.

---

## directory layout

```
.
├── command.h
├── command.cpp
├── pico2.ino
├── pico1.ino
└── pc_host.py
```

---

## protocol (rigorous)

All multi‑byte fields are **little‑endian**.

### PC → Pico2 (USB Serial)

**Frame size: 520 bytes**

| field | bytes | description |
|-----|------:|-------------|
| MAGIC | 2 | constant `0x55AA` |
| SEQ | 4 | frame sequence number (`uint32`) |
| DATA | 512 | packed 4‑bit magnet values |
| CRC16 | 2 | CRC16‑CCITT (init = `0xFFFF`) over **[HDR + DATA]** |

Definitions:
- `HDR_BYTES = 6`
- `DATA_BYTES = 512`
- `CRC_BYTES = 2`
- `FRAME_BYTES = 520`

#### DATA packing rule (host)

Each magnet is represented by a **4‑bit value** in `{0..15}`.

- `15` is **forbidden** (firmware forces that magnet OFF).
- Recommended active range: `0..14`.

Packing rule:
```
packed[i] = (hi << 4) | lo
lo = values[2*i + 0] & 0x0F
hi = values[2*i + 1] & 0x0F
```

This is the exact inverse of `buildX()` on the MCU:
```
X[2*i+0] = packed[i] & 0x0F
X[2*i+1] = (packed[i] >> 4) & 0x0F
```

---

### Pico2 → Pico1 (UART)

**Packet size: 260 bytes**

| field | bytes | description |
|-----|------:|-------------|
| SEQ | 4 | same SEQ as PC frame |
| PAYLOAD | 256 | first half of DATA |

Definitions:
- `UART_SEQ_BYTES = 4`
- `UART_PAYLOAD_BYTES = 256`

---

### ACK (Pico1 → Pico2 → PC)

**Packet size: 7 bytes**

| field | bytes | description |
|-----|------:|-------------|
| ACK_MAGIC | 2 | constant `0x55AA` |
| SEQ | 4 | must match TX frame SEQ |
| STATUS | 1 | result code |

Status convention:
- `1` → OK
- any other value → error (CRC fail, timeout, downstream failure, etc.)

---

## magnet actuation (PCA9685)

### topology

- Two I2C buses per controller:
  - `Wire`  → 32 PCA9685 boards
  - `Wire1` → 32 PCA9685 boards
- Each PCA9685:
  - 16 PWM channels
  - 8 channel pairs → 8 magnets

### addressing

Per I2C bus:
- base address: `0x40`
- board `i` uses address `0x40 + i`
- total range: `0x40 .. 0x5F` (32 boards)

### intensity rule

For each magnet value:

```
intensity = value - 7      // range [-7 .. +7]
```

PWM magnitude:
- `|intensity|` mapped linearly to `[0 .. 4095]`

Polarity routing per channel pair:
- `intensity > 0` → LEFT = pwm, RIGHT = 0
- `intensity < 0` → LEFT = 0, RIGHT = pwm
- `intensity = 0` → both OFF

---

## file responsibilities

### command.h / command.cpp

Shared protocol and utilities:
- exact stream I/O (`readExactBytes`, `writeExactBytes`)
- endian helpers (`rd_u16_le`, `wr_u32_le`, etc.)
- CRC16‑CCITT
- nibble unpacking (`buildX`)
- magnet actuation (`actionX`)
- ACK helpers (`makeAck`, `readAck`)

Rules:
- function signatures **must match exactly** between header and source
- implementation stays in `.cpp` to avoid ODR / duplicate symbol issues

### pico2.ino

- receives framed data from PC
- validates MAGIC and CRC
- forwards first half to Pico1
- applies second half locally
- waits for Pico1 ACK
- sends final ACK to PC

### pico1.ino

- receives UART packet from Pico2
- unpacks and applies magnet commands
- returns ACK

### pc_host.py

- builds TX frames
- computes CRC
- sends frames to Pico2
- validates ACK (magic + seq)
- example pattern generator included

---

## bring‑up checklist

1. Verify I2C wiring and pull‑ups on both buses.
2. Confirm PCA9685 address straps produce `0x40 .. 0x5F`.
3. Flash `pico1.ino` and `pico2.ino`.
4. Run `pc_host.py` on PC.
5. Confirm:
   - ACK received for every frame,
   - SEQ increments correctly,
   - STATUS returns `1`.

---

## tuning notes

- If ACK timeouts occur:
  - increase Pico2 ACK timeout,
  - reduce I2C clock from 1 MHz to 400 kHz.
- For large bus fan‑out:
  - add small inter‑board delays,
  - ensure solid ground and short SDA/SCL runs.

---
