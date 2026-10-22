#!/usr/bin/env python3
"""Replay a YT2 VIDEO recording produced by the STREAM build (M1b spike).

    python3 tools/stream/replay.py STREAM.BIN --out frames/

The recording is a bare sequence of protocol-v0 frames (docs/protocol.md):
a 14-byte header followed by the payload (the first payload byte is the
message type). The spike emits only VIDEO messages with the raw codec.
"""

import argparse
import os
import struct
import sys

from PIL import Image

MAGIC = 0x5954
HDR = struct.Struct("<HBBBBII")
FAMILY_VIDEO = 1
FAMILY_CONTROL = 5

SET_MODE, PALETTE, RECT, FRAME_END, RESET, SCROLL = range(6)

W, H = 320, 200


def read_records(path):
    data = open(path, "rb").read()
    off = 0
    records = []
    while off + HDR.size <= len(data):
        magic, version, family, flags, codec, seq, length = HDR.unpack_from(data, off)
        if magic != MAGIC:
            raise SystemExit(f"bad magic 0x{magic:04x} at offset {off}")
        if version != 0:
            raise SystemExit(f"unsupported protocol version {version}")
        end = off + HDR.size + length
        if end > len(data):
            print(f"replay: truncated record at {off}", file=sys.stderr)
            break
        records.append((family, codec, seq, data[off + HDR.size:end]))
        off = end
    return records, off, len(data)


def decode_runs(data, width, height):
    """Decode run-length coded rows.

    The runs are: 00 nn repeats the previous pixel, 01 nn vv repeats vv,
    02 nn takes nn literal pixels and 03 ends the row.  Returns the rows or
    None when the data does not decode.
    """
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
                previous = row[-1] if count else previous
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
    ap = argparse.ArgumentParser()
    ap.add_argument("recording")
    ap.add_argument("--out", default="frames", help="directory for PNG frames")
    ap.add_argument("--ppm", help="write the final state as a binary PPM (P6)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    records, consumed, total = read_records(args.recording)
    os.makedirs(args.out, exist_ok=True)

    width, height = W, H
    fb = bytearray(width * height)
    pal = bytearray(768)
    last = None
    frames_saved = 0
    frames = 0
    rects = 0
    pal_updates = 0
    control = 0
    scrolls = 0

    def save_frame(tag=None):
        nonlocal last, frames_saved, fb, width, height
        key = (bytes(fb), bytes(pal))
        if key == last:
            return None
        last = key
        # DAC values are 6-bit; expand them to the 8-bit range PIL expects.
        pal8 = bytes((((v & 0x3f) << 2) | ((v & 0x3f) >> 4)) for v in pal)
        img = Image.frombytes("P", (width, height), bytes(fb))
        img.putpalette(pal8)
        name = f"frame_{frames_saved:04d}.png" if tag is None else tag
        img.save(os.path.join(args.out, name))
        frames_saved += 1
        return name

    for family, codec, seq, payload in records:
        if family == FAMILY_CONTROL:
            control += 1
            continue
        if family != FAMILY_VIDEO:
            continue
        if codec not in (0, 1):
            raise SystemExit(f"unsupported codec {codec} at seq {seq}")
        if not payload:
            continue
        kind = payload[0]
        if kind == SET_MODE:
            w, h, bpp = struct.unpack_from("<HHB", payload, 1)
            if bpp != 8 or w == 0 or h == 0:
                raise SystemExit(f"unsupported mode {w}x{h}x{bpp} at seq {seq}")
            if (w, h) != (width, height):
                # The intro runs at 640x480, the game at 320x200; a mode
                # message starts from a blank screen.
                width, height = w, h
                fb = bytearray(width * height)
        elif kind == PALETTE:
            start, count = struct.unpack_from("<HH", payload, 1)
            data = payload[5:5 + 3 * count]
            pal[3 * start:3 * start + len(data)] = data
            pal_updates += 1
        elif kind == RECT:
            x, y, w, h = struct.unpack_from("<HHHH", payload, 1)
            if x + w > width or y + h > height:
                print(f"replay: bad rect at seq {seq}", file=sys.stderr)
                continue
            if codec == 0:
                data = payload[9:9 + w * h]
                if len(data) != w * h:
                    print(f"replay: bad rect at seq {seq}", file=sys.stderr)
                    continue
                rows = [data[row * w:(row + 1) * w] for row in range(h)]
            else:
                rows = decode_runs(payload[9:], w, h)
                if rows is None:
                    print(f"replay: bad rect at seq {seq}", file=sys.stderr)
                    continue
            for row in range(h):
                dst = (y + row) * width + x
                fb[dst:dst + w] = rows[row]
            rects += 1
        elif kind == SCROLL:
            # The whole picture moved by dy rows (positive: down); the rows
            # that scroll in, and the rows that did not move with it, follow
            # as rects in the same frame.
            if len(payload) < 2:
                raise SystemExit(f"short scroll at seq {seq}")
            dy = struct.unpack_from("<b", payload, 1)[0]
            if dy <= -height or dy >= height:
                raise SystemExit(f"scroll {dy} does not fit {height} rows at seq {seq}")
            if dy > 0:
                fb[dy * width:] = fb[:(height - dy) * width]
                fb[:dy * width] = b"\x00" * (dy * width)
            elif dy < 0:
                up = -dy
                fb[:(height - up) * width] = fb[up * width:]
                fb[(height - up) * width:] = b"\x00" * (up * width)
            scrolls += 1
        elif kind == FRAME_END:
            frames += 1
            save_frame()
        elif kind == RESET:
            fb[:] = b"\x00" * (width * height)
        else:
            print(f"replay: unknown message {kind} at seq {seq}", file=sys.stderr)

    last_name = save_frame("final.png")

    if args.ppm:
        # Same bytes the C# client writes: P6 header plus 8-bit RGB, one byte
        # per channel, rows top to bottom.  tests/replay/run.sh compares the
        # two files with cmp.
        pal8 = bytes((((v & 0x3f) << 2) | ((v & 0x3f) >> 4)) for v in pal)
        rgb = bytearray(width * height * 3)
        for i in range(width * height):
            c = fb[i] * 3
            rgb[i * 3:i * 3 + 3] = pal8[c:c + 3]
        with open(args.ppm, "wb") as f:
            f.write(b"P6\n%d %d\n255\n" % (width, height))
            f.write(rgb)

    if not args.quiet:
        print(f"records: {len(records)} ({consumed}/{total} bytes consumed)")
        print(f"palette updates: {pal_updates}, rects: {rects}, frames: {frames}, "
              f"control: {control}")
        print(f"distinct frames saved: {frames_saved} in {args.out}")
        if last_name:
            print(f"final frame: {os.path.join(args.out, last_name)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
