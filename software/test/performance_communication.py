import time
import struct
import serial

# config. - USB
PORT = "/dev/cu.usbmodem3101" 

BAUDRATE = 115200
TIMEOUT  = 0.5

MAGIC     = 0x55AA      # frame header
ACK_MAGIC = 0x33CC      # ack header

DATA_BYTES  = 512
HDR_BYTES   = 6         # MAGIC(2) + SEQ(4)
CRC_BYTES   = 2
FRAME_BYTES = HDR_BYTES + DATA_BYTES + CRC_BYTES  # 520

ACK_BYTES = 7           # ACK_MAGIC(2) + SEQ(4) + STATUS(1)

def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    crc = init
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def read_exact(ser: serial.Serial, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if not chunk:
            break
        buf.extend(chunk)
    return bytes(buf)

def build_frame(seq: int, data512: bytes) -> bytes:
    if len(data512) != DATA_BYTES:
        raise ValueError("data512 must be 512 bytes")
    hdr = struct.pack("<HI", MAGIC, seq)
    crc = crc16_ccitt(data512)
    return hdr + data512 + struct.pack("<H", crc)

def parse_ack(ack7: bytes):
    if len(ack7) != ACK_BYTES:
        return None
    am, seq = struct.unpack("<HI", ack7[:6])
    status = ack7[6]
    return am, seq, status

def main():
    ser = serial.Serial(PORT, BAUDRATE, timeout=TIMEOUT)

    try:
        for seq in range(100):
            # 512 bytes data
            data512 = bytes([(seq + i) & 0xFF for i in range(DATA_BYTES)])

            frame = build_frame(seq, data512)
            if len(frame) != FRAME_BYTES:
                print(seq, "FAIL: frame_len")
                continue

            t0 = time.perf_counter()
            ser.write(frame)
            ser.flush()

            ack = read_exact(ser, ACK_BYTES)
            t1 = time.perf_counter()

            rtt_ms = (t1 - t0) * 1000.0

            parsed = parse_ack(ack)
            if not parsed:
                print(seq, f"FAIL: ack_len={len(ack)} rtt_ms={rtt_ms:.3f}")
                continue

            am, aseq, status = parsed
            if am != ACK_MAGIC:
                print(seq, f"FAIL: ack_magic=0x{am:04X} rtt_ms={rtt_ms:.3f}")
                continue
            if aseq != seq:
                print(seq, f"FAIL: ack_seq={aseq} rtt_ms={rtt_ms:.3f}")
                continue

            print(seq, f"OK status={status} rtt_ms={rtt_ms:.3f}")

    finally:
        ser.close()

if __name__ == "__main__":
    main()
