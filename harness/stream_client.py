#!/usr/bin/env python3
"""Replay a YT2 VIDEO recording produced by the STREAM build (M1b spike).

    python3 harness/stream_client.py STREAM.BIN --out frames/

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

SET_MODE, PALETTE, RECT, FRAME_END, RESET = range(5)

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
            print(f"stream_client: truncated record at {off}", file=sys.stderr)
            break
        records.append((family, codec, seq, data[off + HDR.size:end]))
        off = end
    return records, off, len(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("recording")
    ap.add_argument("--out", default="frames", help="directory for PNG frames")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    records, consumed, total = read_records(args.recording)
    os.makedirs(args.out, exist_ok=True)

    fb = bytearray(W * H)
    pal = bytearray(768)
    last = None
    frames_saved = 0
    frames = 0
    rects = 0
    pal_updates = 0

    def save_frame(tag=None):
        nonlocal last, frames_saved
        key = (bytes(fb), bytes(pal))
        if key == last:
            return None
        last = key
        # DAC values are 6-bit; expand them to the 8-bit range PIL expects.
        pal8 = bytes((((v & 0x3f) << 2) | ((v & 0x3f) >> 4)) for v in pal)
        img = Image.frombytes("P", (W, H), bytes(fb))
        img.putpalette(pal8)
        name = f"frame_{frames_saved:04d}.png" if tag is None else tag
        img.save(os.path.join(args.out, name))
        frames_saved += 1
        return name

    for family, codec, seq, payload in records:
        if family != FAMILY_VIDEO:
            continue
        if codec != 0:
            raise SystemExit(f"unsupported codec {codec} at seq {seq}")
        if not payload:
            continue
        kind = payload[0]
        if kind == SET_MODE:
            w, h, bpp = struct.unpack_from("<HHB", payload, 1)
            if (w, h, bpp) != (W, H, 8):
                raise SystemExit(f"unsupported mode {w}x{h}x{bpp} at seq {seq}")
        elif kind == PALETTE:
            start, count = struct.unpack_from("<HH", payload, 1)
            data = payload[5:5 + 3 * count]
            pal[3 * start:3 * start + len(data)] = data
            pal_updates += 1
        elif kind == RECT:
            x, y, w, h = struct.unpack_from("<HHHH", payload, 1)
            data = payload[9:9 + w * h]
            if x + w > W or y + h > H or len(data) != w * h:
                print(f"stream_client: bad rect at seq {seq}", file=sys.stderr)
                continue
            for row in range(h):
                dst = (y + row) * W + x
                fb[dst:dst + w] = data[row * w:(row + 1) * w]
            rects += 1
        elif kind == FRAME_END:
            frames += 1
            save_frame()
        elif kind == RESET:
            fb[:] = b"\x00" * (W * H)
        else:
            print(f"stream_client: unknown message {kind} at seq {seq}", file=sys.stderr)

    last_name = save_frame("final.png")

    if not args.quiet:
        print(f"records: {len(records)} ({consumed}/{total} bytes consumed)")
        print(f"palette updates: {pal_updates}, rects: {rects}, frames: {frames}")
        print(f"distinct frames saved: {frames_saved} in {args.out}")
        if last_name:
            print(f"final frame: {os.path.join(args.out, last_name)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
