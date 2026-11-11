#!/usr/bin/env python3
"""Check a B2 folder-link recording end to end.

    check_stream.py OUTPUT.BIN EXPECTED_FIRST_HEX [MIN_FRAMES]

Walks the YT2B container, requires non-decreasing stamps and a contiguous
engine sequence, decodes VIDEO (set_mode/palette/rect/frame_end) to indexed
frames, and requires at least MIN_FRAMES (default 1) with the first frame's
FNV-1a 64 equal to EXPECTED_FIRST_HEX.  An independent v0 read of the same
bytes the C# client and replay.py decode.
"""

import struct
import sys

FNV_BASIS = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def fnv(data):
    h = FNV_BASIS
    for b in data:
        h = ((h ^ b) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    if len(sys.argv) < 3:
        print("usage: check_stream.py OUTPUT.BIN EXPECTED_FIRST_HEX "
              "[MIN_FRAMES]")
        return 2
    path, want = sys.argv[1], int(sys.argv[2], 16)
    minimum = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    data = open(path, "rb").read()
    if data[:4] != b"YT2B":
        print("stream: not a YT2B file")
        return 1
    magic, version, header_size, timebase, direction = struct.unpack(
        "<4sHHIB3x", data[:16])
    if (magic, version, header_size, timebase, direction) != (
            b"YT2B", 1, 16, 1193182, 0):
        print("stream: bad YT2B header")
        return 1
    off = 16
    last_stamp = None
    want_seq = None
    width = height = 0
    framebuffer = bytearray()
    frames = []
    records = 0
    while off + 8 <= len(data):
        stamp, size = struct.unpack("<II", data[off:off + 8])
        frame = data[off + 8:off + 8 + size]
        if len(frame) != size or len(frame) < 14:
            print(f"stream: truncated record at {off}")
            return 1
        if last_stamp is not None and \
                ((stamp - last_stamp) & 0xFFFFFFFF) >= 0x80000000:
            print(f"stream: stamp moved backwards at {off}")
            return 1
        last_stamp = stamp
        if frame[:2] != b"\x54\x59" or frame[2] != 0:
            print(f"stream: bad v0 header at {off}")
            return 1
        seq = struct.unpack("<I", frame[6:10])[0]
        if want_seq is None:
            want_seq = seq
        if seq != want_seq:
            print(f"stream: seq {seq}, want {want_seq} at {off}")
            return 1
        want_seq = (want_seq + 1) & 0xFFFFFFFF
        family, kind = frame[3], frame[14]
        payload = frame[15:]
        if family == 1 and kind == 0:
            width, height = struct.unpack("<HH", payload[:4])
            framebuffer = bytearray(width * height)
        elif family == 1 and kind == 2:
            x, y, w, h = struct.unpack("<HHHH", payload[:8])
            pixels = payload[8:8 + w * h]
            if len(pixels) != w * h or x + w > width or y + h > height:
                print(f"stream: bad rect at {off}")
                return 1
            for row in range(h):
                base = (y + row) * width + x
                framebuffer[base:base + w] = pixels[row * w:(row + 1) * w]
        elif family == 1 and kind == 3:
            frames.append(bytes(framebuffer))
        records += 1
        off += 8 + size
    if off != len(data):
        print(f"stream: {len(data) - off} trailing bytes")
        return 1
    if len(frames) < minimum:
        print(f"stream: {len(frames)} frames, want {minimum}")
        return 1
    got = fnv(frames[0])
    if got != want:
        print(f"stream: first frame {got:016X}, want {want:016X}")
        return 1
    print(f"stream: OK ({records} records, {len(frames)} frames, "
          f"first {got:016X})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
