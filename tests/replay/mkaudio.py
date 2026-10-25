#!/usr/bin/env python3
"""Build tests/data/sample-audio.bin.

    python3 tests/replay/mkaudio.py tests/data/sample-audio.bin

The fixture is a small protocol-v0 stream that interleaves the AUDIO family with VIDEO: a mode, a
palette, two rows of picture, one `opl_write`, one `opl_batch`, one `opl_batch_timed`, one
`audio_ctrl`, one speaker on/off pair, and frame boundaries.  `tests/replay/run.sh` replays it through the C# client and the Python decoder; both
must render the same final frame, which proves the AUDIO messages are consumed without disturbing
the picture path (and that both sides parse the same payload layout, docs/protocol.md family 2).
"""

import argparse
import struct

MAGIC = 0x5954
HDR = struct.Struct("<HBBBBII")
VIDEO, AUDIO, CONTROL = 1, 2, 5


def frame(seq, family, payload, codec=0):
    return HDR.pack(MAGIC, 0, family, 0, codec, seq, len(payload)) + bytes(payload)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    args = ap.parse_args()

    seq = 0
    out = bytearray()

    def add(family, payload, codec=0):
        nonlocal seq
        seq += 1
        out.extend(frame(seq, family, payload, codec))

    # CONTROL hello(role=engine, version=0)
    add(CONTROL, [0, 0, 0])

    # VIDEO set_mode(320, 200, 8)
    add(VIDEO, [0] + list(struct.pack("<HHB", 320, 200, 8)))

    # VIDEO palette(0, 256): a simple ramp in 6-bit DAC values
    pal = bytearray()
    for i in range(256):
        pal += bytes((i & 0x3F, (i * 2) & 0x3F, (255 - i) & 0x3F))
    add(VIDEO, [1] + list(struct.pack("<HH", 0, 256)) + list(pal))

    # VIDEO rect(0, 0, 320, 6), raw rows
    rows = bytearray()
    for y in range(6):
        rows += bytes(((x + y) & 0xFF) for x in range(320))
    add(VIDEO, [2] + list(struct.pack("<HHHH", 0, 0, 320, 6)) + list(rows))

    # AUDIO: one write, one batch, one control message (the FM register base is 0x20 here)
    add(AUDIO, [0] + list(struct.pack("<HBB", 0x228, 0x20, 0x01)))   # opl_write
    batch = [1, 3]
    for reg, val in ((0x23, 0x21), (0x40, 0x10), (0x43, 0x00)):
        batch += list(struct.pack("<HBB", 0x228, reg, val))
    add(AUDIO, batch)                                               # opl_batch
    # opl_batch_timed: port + count, then (PIT stamp, register, value); the
    # stamps are 10 ms apart, which is one SBFMDRV driver tick at 96 Hz.
    timed = bytearray([3])
    timed += struct.pack("<HB", 0x228, 3)
    for i, (reg, val) in enumerate(((0x43, 0x00), (0x63, 0x00), (0x83, 0x00))):
        timed += struct.pack("<IBB", i * 12429, reg, val)
    add(AUDIO, list(timed))                                         # opl_batch_timed
    add(AUDIO, [2] + list(struct.pack("<HHB", 44100, 0, 1)))  # audio_ctrl
    # speaker on/off: PIT channel 2 divisor 662 (~1.8 kHz), 220 ms apart, the
    # same shape fn_SpeakerBeep streams.  The stamps follow the timed batch
    # (stream stamps never go backwards).
    add(AUDIO, [4, 1] + list(struct.pack("<HI", 662, 300000)))   # speaker on
    add(AUDIO, [4, 0] + list(struct.pack("<HI", 0, 562500)))     # speaker off

    add(VIDEO, [3])                                        # frame_end

    # a second frame: the remaining rows, then frame_end
    rows = bytearray()
    for y in range(6, 12):
        rows += bytes(((x ^ y) & 0xFF) for x in range(320))
    add(VIDEO, [2] + list(struct.pack("<HHHH", 0, 6, 320, 6)) + list(rows))
    add(VIDEO, [3])                                        # frame_end

    with open(args.out, "wb") as f:
        f.write(out)
    print(f"{args.out}: {len(out)} bytes, {seq} records")


if __name__ == "__main__":
    main()
