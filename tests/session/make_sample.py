#!/usr/bin/env python3
"""Build/check the timed STREAM.BIN sample from the raw video fixture."""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tests/data/sample-video.bin"
OUTPUT = ROOT / "tests/data/sample-timed-stream.bin"
FRAME = struct.Struct("<HBBBBII")
BIN_HEADER = struct.Struct("<4sHHIB3x")
BIN_EVENT = struct.Struct("<II")
PIT_RATE = 1_193_182
FRAME_END = 3


def build() -> bytes:
    source = SOURCE.read_bytes()
    output = bytearray(BIN_HEADER.pack(b"YT2B", 1, BIN_HEADER.size, PIT_RATE, 0))
    offset = 0
    stamp = 0
    while offset < len(source):
        if len(source) - offset < FRAME.size:
            raise ValueError(f"truncated v0 header at {offset}")
        magic, version, family, flags, codec, sequence, size = FRAME.unpack_from(source, offset)
        end = offset + FRAME.size + size
        if magic != 0x5954 or version != 0 or end > len(source):
            raise ValueError(f"invalid v0 frame at {offset}")
        raw = source[offset:end]
        output += BIN_EVENT.pack(stamp, len(raw))
        output += raw
        if family == 1 and size and source[offset + FRAME.size] == FRAME_END:
            stamp += PIT_RATE // 24
        offset = end
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=pathlib.Path, default=OUTPUT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = build()
    if args.check:
        if not args.out.is_file() or args.out.read_bytes() != data:
            print(f"timed sample differs: {args.out}", file=sys.stderr)
            return 1
        print(f"timed sample: OK ({len(data)} bytes)")
        return 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"timed sample: wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
