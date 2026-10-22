#!/usr/bin/env python3
"""Build the pan fixtures: one pan recorded as rows, one as scroll messages.

    mkstream.py tests/data

Two recordings of the same 320x200 scene sliding up six rows and back down
six: `sample-pan-rows.bin` sends every changed row, `sample-pan-scroll.bin`
sends one scroll message and the row that slides in.  Both are legal
recordings of the same pictures, so the decoders must agree with each other
and the two files must render identically - that is what check_stream.py and
tests/replay/run.sh require.
"""
import os
import struct
import sys

MAGIC = 0x5954
HDR = struct.Struct("<HBBBBII")
VIDEO = 1
SET_MODE, PALETTE, RECT, FRAME_END, RESET, SCROLL = range(6)
W, H = 320, 200
STEPS = [-1] * 6 + [1] * 6          # up six rows, then back down


def world_row(y):
    """A row with 16-pixel runs, so the run codec keeps the file small."""
    return bytes((((y // 4) * 11 + (x // 16) * 5 + (y % 4)) & 0xFF) for x in range(W))


def encode_row(row):
    out = bytearray()
    literals = bytearray()

    def flush():
        at = 0
        while at < len(literals):
            chunk = literals[at:at + 255]
            out.append(2)
            out.append(len(chunk))
            out.extend(chunk)
            at += 255
        literals.clear()

    at = 0
    while at < len(row):
        end = at
        while end < len(row) and row[end] == row[at] and end - at < 255:
            end += 1
        if end - at >= 3:
            flush()
            out.append(1)
            out.append(end - at)
            out.append(row[at])
        else:
            for i in range(at, end):
                literals.append(row[i])
                if len(literals) >= 255:
                    flush()
        at = end
    flush()
    out.append(3)
    return bytes(out)


def world_frames():
    """The screen for every step, over a world six rows taller than a screen."""
    world = [world_row(y) for y in range(H + len(STEPS) + 1)]
    frames = []
    top = len(STEPS)
    for step in STEPS:
        top += step
        frames.append([world[top + y] for y in range(H)])
    return frames


class Writer:
    def __init__(self):
        self.out = bytearray()
        self.seq = 0

    def message(self, kind, payload, codec=0):
        body = bytes([kind]) + payload
        self.out += HDR.pack(MAGIC, 0, VIDEO, 0, codec, self.seq, len(body))
        self.out += body
        self.seq += 1

    def palette(self):
        data = bytearray()
        for i in range(256):
            data += bytes((i & 0x3F, (i * 2) & 0x3F, (i * 3) & 0x3F))
        self.message(PALETTE, struct.pack("<HH", 0, 256) + bytes(data))

    def set_mode(self):
        self.message(SET_MODE, struct.pack("<HHB", W, H, 8))

    def rect(self, y, row):
        self.message(RECT, struct.pack("<HHHH", 0, y, W, 1) + encode_row(row), codec=1)

    def scroll(self, dy):
        self.message(SCROLL, struct.pack("<b", dy))

    def frame_end(self):
        self.message(FRAME_END, b"")


def write_rows(path, frames):
    w = Writer()
    w.set_mode()
    w.palette()
    prev = None
    for frame in frames:
        for y in range(H):
            if prev is None or prev[y] != frame[y]:
                w.rect(y, frame[y])
        w.frame_end()
        prev = frame
    open(path, "wb").write(w.out)
    return len(w.out)


def write_scroll(path, frames):
    w = Writer()
    w.set_mode()
    w.palette()
    prev = None
    for frame in frames:
        if prev is None:
            for y in range(H):
                w.rect(y, frame[y])
        else:
            moved = next((dy for dy in (-1, 1)
                          if all(frame[y] == prev[y - dy]
                                 for y in range(max(0, dy), min(H, H + dy)))), None)
            if moved is None:
                for y in range(H):
                    if prev[y] != frame[y]:
                        w.rect(y, frame[y])
            else:
                w.scroll(moved)
                # the rows the scroll does not cover, and any other change
                for y in range(H):
                    if moved > 0 and y < moved or moved < 0 and y >= H + moved:
                        w.rect(y, frame[y])
                    elif prev[y - moved] != frame[y]:
                        w.rect(y, frame[y])
        w.frame_end()
        prev = frame
    open(path, "wb").write(w.out)
    return len(w.out)


def main(argv):
    if len(argv) != 1:
        raise SystemExit(__doc__)
    out = argv[0]
    os.makedirs(out, exist_ok=True)
    frames = world_frames()
    rows = write_rows(os.path.join(out, "sample-pan-rows.bin"), frames)
    scroll = write_scroll(os.path.join(out, "sample-pan-scroll.bin"), frames)
    print(f"mkstream: sample-pan-rows.bin {rows} bytes, "
          f"sample-pan-scroll.bin {scroll} bytes, {len(frames)} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
