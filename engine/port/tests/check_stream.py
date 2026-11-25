#!/usr/bin/env python3
"""Check a B2 folder-link recording end to end.

    check_stream.py OUTPUT.BIN EXPECTED_FIRST_HEX [MIN_FRAMES]
    check_stream.py FIXTURE.BIN --all

Walks the YT2B container (or a raw v0 fixture), requires non-decreasing
stamps and a contiguous engine sequence, decodes VIDEO
(set_mode/palette/rect/scroll/frame_end, rects in codec 0 or 1) to indexed
frames, and requires at least MIN_FRAMES (default 1) with the first
frame's FNV-1a 64 equal to EXPECTED_FIRST_HEX.  TICK traffic is checked
too: every frame_end in a tick-carrying stream is immediately preceded by
a tick, tick numbers rise modulo 2^32, and pace never changes.  With --all
it prints every frame's hash instead of checking the first.  An
independent v0 read of the same bytes the C# client and replay.py decode.
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


def decode_runs(data, width, height):
    """Decode codec-1 row runs; None when the bytes do not decode."""
    rows = []
    at = 0
    for _ in range(height):
        row = bytearray()
        previous = 0
        have = False
        while len(row) < width:
            if at >= len(data):
                return None
            op = data[at]
            at += 1
            if op == 3:
                break
            if op == 0:
                if not have or at >= len(data):
                    return None
                count = data[at]
                at += 1
                value = previous
            elif op == 1:
                if at + 1 >= len(data):
                    return None
                count = data[at]
                value = data[at + 1]
                at += 2
            elif op == 2:
                if at >= len(data):
                    return None
                count = data[at]
                at += 1
                if at + count > len(data) or len(row) + count > width:
                    return None
                row += data[at:at + count]
                at += count
                if count:
                    previous = row[-1]
                    have = True
                continue
            else:
                return None
            if count == 0 or len(row) + count > width:
                return None
            row += bytes([value]) * count
            previous = value
            have = True
        if len(row) != width:
            return None
        rows.append(bytes(row))
    return rows


def main():
    if len(sys.argv) < 3:
        print("usage: check_stream.py OUTPUT.BIN EXPECTED_FIRST_HEX "
              "[MIN_FRAMES]")
        print("       check_stream.py FIXTURE.BIN --all")
        return 2
    dump_all = len(sys.argv) > 2 and sys.argv[2] == "--all"
    path = sys.argv[1]
    want = 0 if dump_all else int(sys.argv[2], 16)
    minimum = 1
    if not dump_all and len(sys.argv) > 3:
        minimum = int(sys.argv[3])
    data = open(path, "rb").read()
    if data[:4] == b"YT2B":
        magic, version, header_size, timebase, direction = struct.unpack(
            "<4sHHIB3x", data[:16])
        if (magic, version, header_size, timebase, direction) != (
                b"YT2B", 1, 16, 1193182, 0):
            print("stream: bad YT2B header")
            return 1
        off = 16
        timed = True
    elif data[:2] == b"\x54\x59":
        off = 0
        timed = False
    else:
        print("stream: neither YT2B nor raw v0")
        return 1
    last_stamp = None
    want_seq = None
    width = height = 0
    framebuffer = bytearray()
    frames = []
    records = 0
    tick_count = 0
    pace_count = 0
    pace_ms = None
    last_tick = None
    last_was_tick = False
    bare_ends = 0
    while True:
        if timed:
            if off + 8 > len(data):
                break
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
        else:
            if off + 14 > len(data):
                break
            size = 14 + struct.unpack("<I", data[off + 10:off + 14])[0]
            frame = data[off:off + size]
            if len(frame) != size:
                print(f"stream: truncated record at {off}")
                return 1
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
        codec = frame[5]
        payload = frame[15:]
        if family == 1 and kind == 0:
            width, height = struct.unpack("<HH", payload[:4])
            framebuffer = bytearray(width * height)
        elif family == 1 and kind == 2:
            x, y, w, h = struct.unpack("<HHHH", payload[:8])
            if codec == 0:
                pixels = payload[8:8 + w * h]
                if len(pixels) != w * h:
                    print(f"stream: bad rect at {off}")
                    return 1
                rows = [pixels[row * w:(row + 1) * w] for row in range(h)]
            elif codec == 1:
                rows = decode_runs(payload[8:], w, h)
                if rows is None:
                    print(f"stream: bad runs rect at {off}")
                    return 1
            else:
                print(f"stream: bad codec {codec} at {off}")
                return 1
            if x + w > width or y + h > height:
                print(f"stream: bad rect at {off}")
                return 1
            for row in range(h):
                base = (y + row) * width + x
                framebuffer[base:base + w] = rows[row]
        elif family == 1 and kind == 3:
            if not last_was_tick:
                bare_ends += 1
            frames.append(bytes(framebuffer))
        elif family == 4 and kind == 0:
            if len(payload) < 4:
                print(f"stream: short tick at {off}")
                return 1
            n = struct.unpack("<I", payload[:4])[0]
            if last_tick is not None and \
                    ((n - last_tick) & 0xFFFFFFFF) >= 0x80000000:
                print(f"stream: tick moved backwards at {off}")
                return 1
            last_tick = n
            tick_count += 1
        elif family == 4 and kind == 1:
            if len(payload) < 2:
                print(f"stream: short pace at {off}")
                return 1
            ms = struct.unpack("<H", payload[:2])[0]
            if pace_ms is not None and ms != pace_ms:
                print(f"stream: pace changed at {off}")
                return 1
            pace_ms = ms
            pace_count += 1
        elif family == 1 and kind == 5:
            if len(payload) < 1:
                print(f"stream: short scroll at {off}")
                return 1
            dy = struct.unpack("b", payload[:1])[0]
            if dy <= -height or dy >= height:
                print(f"stream: scroll {dy} does not fit at {off}")
                return 1
            if dy > 0:
                framebuffer[dy * width:] = framebuffer[:len(framebuffer) -
                                                       dy * width]
                framebuffer[:dy * width] = b"\x00" * (dy * width)
            elif dy < 0:
                up = -dy
                framebuffer[:len(framebuffer) - up * width] = \
                    framebuffer[up * width:]
                framebuffer[len(framebuffer) - up * width:] = \
                    b"\x00" * (up * width)
        last_was_tick = family == 4 and kind == 0
        records += 1
        off += size if not timed else 8 + size
    if off != len(data):
        print(f"stream: {len(data) - off} trailing bytes")
        return 1
    if len(frames) < minimum:
        print(f"stream: {len(frames)} frames, want {minimum}")
        return 1
    if tick_count > 0 and bare_ends > 0:
        print(f"stream: {bare_ends} frames without tick")
        return 1
    if dump_all:
        for frame in frames:
            print(f"{fnv(frame):016X}")
        return 0
    got = fnv(frames[0])
    if got != want:
        print(f"stream: first frame {got:016X}, want {want:016X}")
        return 1
    print(f"stream: OK ({records} records, {len(frames)} frames, "
          f"first {got:016X}, ticks={tick_count}, paces={pace_count})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
