
![Group 119](https://github.com/user-attachments/assets/a626a442-d555-4642-b34e-d9ad59557a28)

# Firmware
- firmware/pico1
- firmware/pico2
- firmware/debug

## pico1
1. From UART (pico2) recieve 512 electromagnet signal
2. Action1: send action to i2c0, i2c1

## pico2
1. From USB (PC) recieve 1024 electromagnet signal
2. Send 512 electromagnet signal to pico1
3. Action: send action to i2c0, i2c1

## debug 
Each `.ino` file is designed for a specific debugging purpose:

- **PCAAddressChecker**  
  Verifies I²C communication and detects connected PCA9685 devices on the bus.  
  Used to confirm correct wiring and address configuration.

- **debug_i2c_bus**  
  Performs one-by-one testing of individual electromagnets on a single I²C bus.  
  Used to validate channel mapping and basic actuation functionality.

- **goToAddress**  
  Connects to a specific PCA9685 device and activates only the selected electromagnet.  
  Used to debug targeting, addressing, and selective channel control.


![image 63](https://github.com/user-attachments/assets/77675a13-d66a-4906-95dd-097407e6bcdc)



# Dual-Pico Electromagnet Control Firmware

This firmware drives a **1,024-channel electromagnet array** using two Raspberry Pi Pico boards (`pico2` and `pico1`) and multiple PCA9685 PWM driver boards. A PC streams packed command frames over USB to `pico2`. The frame is split between the two Picos, unpacked into per-channel commands, and applied to the PCA9685 devices over two I2C buses per Pico.

The system is designed for:

* **Zero data loss** (all 512 bytes are pure magnet data)
* **Deterministic routing** across USB → UART → I2C
* **Reliable execution** using frame numbering (SEQ), CRC, and ACK handshaking

---

## System Overview

Each control frame represents commands for **1,024 electromagnets**.

* The PC sends **512 bytes of pure data per frame** over USB.
* Each byte encodes **two 4-bit channel commands** (two nibbles).
* `pico2` receives the full frame, forwards the first half to `pico1` over UART, and processes the second half locally.
* Each Pico controls **512 electromagnets** using two I2C buses.
* Each I2C bus drives **32 PCA9685 boards**, with **8 electromagnets per PCA** (2 PWM channels per magnet).

Mapping summary:

* 512 bytes → 1,024 channels
* 256 bytes per Pico → 512 channels per Pico
* 256 channels per I2C bus

---

## Communication Protocol (Reliable Framing)

To preserve all 512 bytes of magnet data, framing information is sent **outside** the data payload.

### PC → pico2 (USB frame)

```
[MAGIC(2)] [SEQ(4)] [DATA(512)] [CRC16(2)]   → total 520 bytes
```

* `MAGIC` : frame sync word (0x55AA, little-endian)
* `SEQ`   : 32-bit frame counter
* `DATA`  : 512 bytes (pure magnet data, no header inside)
* `CRC16` : CRC-CCITT over DATA only

### pico2 → pico1 (UART)

```
[SEQ(4)] [DATA_HALF(256)]
```

### pico1 → pico2 (ACK over UART)

```
[ACK_MAGIC(2)] [SEQ(4)] [STATUS(1)]   → 7 bytes
```

### pico2 → PC (USB ACK)

The ACK from `pico1` is forwarded unchanged to the PC.
The PC sends the **next frame only after receiving this ACK** (stop-and-wait protocol).

This guarantees:

* No frame overlap
* Correct frame ordering
* Automatic retry on timeout or CRC failure

---

## Data Format

Each byte from the PC contains two channel commands:

* Low nibble (bits 3–0): first channel
* High nibble (bits 7–4): second channel

Each channel command `Xi ∈ {0…15}` encodes polarity and magnitude:

* `0…7`   → negative polarity, magnitude `0…7`
* `8…15`  → positive polarity, magnitude `Xi − 8`

Magnitude `m ∈ {0…7}` is mapped linearly to a 12-bit PCA9685 PWM value:

```
PWM = m × (4095 / 7)
```

Polarity is implemented by selecting which of the two PCA9685 channels assigned to a magnet is driven.

---

## pico2 Role (USB Receiver + Dispatcher + Local Driver)

`pico2` is the main entry point and protocol master.

For each frame:

1. **Receive frame from PC (USB)**

   * Read `MAGIC + SEQ + DATA(512) + CRC16`.
   * Verify `MAGIC` and CRC.

2. **Forward to pico1 (UART)**

   * Send `SEQ(4)` followed by the **first 256 bytes** of DATA.

3. **Local processing**

   * Use the remaining 256 bytes.
   * Unpack into 512 channel commands.
   * Apply outputs over `Wire` (I2C0) and `Wire1` (I2C1).

4. **Wait for pico1 ACK**

   * Block until `[ACK_MAGIC + SEQ + STATUS]` is received or timeout.

5. **Forward ACK to PC**

   * Send the same 7-byte ACK over USB.

Only after the PC receives this ACK does it transmit the next frame.

---

## pico1 Role (UART Receiver + Local Driver)

`pico1` mirrors the local-driver path of `pico2`.

For each frame:

1. **Receive from pico2 (UART)**

   * Read `SEQ(4)`.
   * Read 256 bytes of data.

2. **Unpack**

   * Convert 256 packed bytes into 512 channel commands.

3. **Apply outputs**

   * Drive PCA9685 devices over `Wire` and `Wire1`.

4. **Send ACK**

   * Transmit `[ACK_MAGIC + SEQ + STATUS]` back to `pico2`.

---

## Shared Command Layer (`command.h` / `command.cpp`)

Both Picos use a common command layer providing:

### Blocking Chunk Read

```cpp
readExactBytes(Stream& s, uint8_t* dst, int n);
```

Reads exactly `n` bytes from any Arduino `Stream` (USB or UART).
Internally uses `available()` + `readBytes()` for **chunk-based reads**, avoiding slow 1-byte loops.

---

### Unpacking Packed Frames

```cpp
buildX(const uint8_t* packed256, uint8_t* X512);
```

* Converts 256 packed bytes into 512 channel commands.
* Each byte produces two 4-bit values.

---

### PCA9685 Output Mapping

```cpp
actionX(TwoWire& bus0, TwoWire& bus1, const uint8_t* X512);
```

* Splits 512 channel commands into two groups of 256.
* Sends the first 256 commands over `bus0` and the second 256 over `bus1`.
* PCA mapping:

  * Base address: `0x40`
  * 32 devices per bus (`0x40 … 0x5F`)
  * 8 electromagnets per PCA (2 PWM channels per magnet)

For each magnet:

* Polarity is selected by choosing channel A or B.
* PWM OFF count is proportional to the commanded magnitude.

---

### CRC and Reliability

* CRC16-CCITT (polynomial 0x1021, init 0xFFFF) protects the 512-byte payload.
* Frames with invalid CRC are dropped silently (PC will timeout and resend).
* SEQ ensures correct ACK matching and ordering.

Reference protocols:

* MAVLink framing: [https://mavlink.io/en/guide/serialization.html](https://mavlink.io/en/guide/serialization.html)
* PPP framing (RFC 1662): [https://www.rfc-editor.org/rfc/rfc1662](https://www.rfc-editor.org/rfc/rfc1662)
* CRC overview: [https://en.wikipedia.org/wiki/Cyclic_redundancy_check](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)

---

## Performance Characteristics

* USB → pico2: chunked blocking read, near maximum CDC throughput
* pico2 → pico1 UART: 921600 baud (configurable)
* I2C buses: up to 1 MHz, chunked 32-byte transfers
* Stop-and-wait ACK protocol ensures correctness at the cost of one RTT per frame

This design prioritizes **correctness and determinism** over raw throughput.
Sliding-window or pipelined modes can be added later if higher frame rates are required.

---

## Summary

This firmware implements a reliable, high-fan-out electromagnet control pipeline:

PC → USB → pico2 → UART → pico1 → I2C0/I2C1 → PCA9685 → 1,024 electromagnets

with:

* No data loss (pure 512-byte payload)
* Deterministic routing
* CRC-verified frames
* ACK-based flow control

Designed for large-scale, real-t

