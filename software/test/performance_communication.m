%% =======================
% User configuration
%% =======================
PORT    = "/dev/cu.usbmodemSN234567892";   % <-- change
BAUD    = 115200;
TIMEOUT = 0.5;                             % seconds (ACK wait)

%% =======================
% Protocol constants
%% =======================
MAGIC     = hex2dec('55AA');   % uint16
ACK_MAGIC = hex2dec('33CC');   % uint16

DATA_BYTES  = 512;
HDR_BYTES   = 6;               % MAGIC(2) + SEQ(4)
CRC_BYTES   = 2;
FRAME_BYTES = HDR_BYTES + DATA_BYTES + CRC_BYTES;  % 520

ACK_BYTES   = 7;               % ACK_MAGIC(2) + SEQ(4) + STATUS(1)

%% Open serial
ser = serialport(PORT, BAUD, "Timeout", TIMEOUT);
flush(ser);  % clear buffers

try
    for seq = uint32(0:99)
        % 512 bytes data: (seq+i)&0xFF, i=0..511
        data512 = uint8(mod(double(seq) + (0:DATA_BYTES-1), 256));

        frame = build_frame(seq, data512, MAGIC);
        if numel(frame) ~= FRAME_BYTES
            fprintf("%u FAIL: frame_len\n", seq);
            continue;
        end

        t0 = tic;
        write(ser, frame, "uint8");
        % serialport write is blocking enough; flush not required, but ok to keep buffers clean
        % flush(ser);

        ack = read_exact(ser, ACK_BYTES);
        rtt_ms = toc(t0) * 1000.0;

        if numel(ack) ~= ACK_BYTES
            fprintf("%u FAIL: ack_len=%d rtt_ms=%.3f\n", seq, numel(ack), rtt_ms);
            continue;
        end

        [am, aseq, status] = parse_ack(ack);

        if am ~= uint16(ACK_MAGIC)
            fprintf("%u FAIL: ack_magic=0x%04X rtt_ms=%.3f\n", seq, am, rtt_ms);
            continue;
        end
        if aseq ~= seq
            fprintf("%u FAIL: ack_seq=%u rtt_ms=%.3f\n", seq, aseq, rtt_ms);
            continue;
        end

        fprintf("%u OK status=%u rtt_ms=%.3f\n", seq, status, rtt_ms);
    end
catch ME
    disp(ME.message);
end

clear ser;  % closes serialport

%% =======================
% Local functions
%% =======================

function out = build_frame(seq, data512, MAGIC)
    DATA_BYTES = 512;
    if numel(data512) ~= DATA_BYTES
        error("data512 must be 512 bytes");
    end
    hdr = [u16le(uint16(MAGIC)), u32le(uint32(seq))];        % 2 + 4 bytes
    crc = crc16_ccitt(data512, uint16(hex2dec('FFFF')));    % over data only (matches your python)
    out = [hdr, data512(:).', u16le(crc)];                  % row vector uint8
end

function [am, seq, status] = parse_ack(ack7)
    am     = le_u16(ack7(1:2));
    seq    = le_u32(ack7(3:6));
    status = uint8(ack7(7));
end

function buf = read_exact(ser, n)
    buf = uint8([]);
    while numel(buf) < n
        need = n - numel(buf);
        if ser.NumBytesAvailable <= 0
            % If nothing arrives, read() will block until Timeout then error.
            % We instead attempt a small read if possible.
            pause(0.001);
        end
        k = min(need, max(ser.NumBytesAvailable, 1));
        try
            chunk = read(ser, k, "uint8");
        catch
            % timeout or read error -> break, caller will treat as short ACK
            break;
        end
        buf = [buf, chunk(:).']; %#ok<AGROW>
    end
end

function crc = crc16_ccitt(data, init)
    % CRC16-CCITT poly 0x1021, init 0xFFFF (bitwise, MSB-first), matches python code
    crc = uint16(init);
    for i = 1:numel(data)
        b = uint16(data(i));
        crc = bitxor(crc, bitshift(b, 8));
        for k = 1:8
            if bitand(crc, uint16(hex2dec('8000')))
                crc = bitand(bitxor(bitshift(crc, 1), uint16(hex2dec('1021'))), uint16(hex2dec('FFFF')));
            else
                crc = bitand(bitshift(crc, 1), uint16(hex2dec('FFFF')));
            end
        end
    end
end

% --- endian helpers (explicit, independent of host endianness) ---
function b = u16le(x)
    x = uint16(x);
    b = uint8([bitand(x,255), bitand(bitshift(x,-8),255)]);
end

function b = u32le(x)
    x = uint32(x);
    b = uint8([ ...
        bitand(x,255), ...
        bitand(bitshift(x,-8),255), ...
        bitand(bitshift(x,-16),255), ...
        bitand(bitshift(x,-24),255)]);
end

function x = le_u16(b)
    b = uint16(b);
    x = b(1) + bitshift(b(2),8);
end

function x = le_u32(b)
    b = uint32(b);
    x = b(1) + bitshift(b(2),8) + bitshift(b(3),16) + bitshift(b(4),24);
end
