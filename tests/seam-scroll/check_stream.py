#!/usr/bin/env python3
"""Check that the row and the scroll recordings of one pan decode the same.

    check_stream.py tests/data

`sample-pan-rows.bin` and `sample-pan-scroll.bin` (mkstream.py) carry the same
twelve pictures: the first sends changed rows, the second sends a scroll
message plus the rows that slide in.  Decode both the way the client does
(docs/protocol.md) and require the same picture after every frame, that the
scroll recording really uses scroll messages, and that the row recording does
not.  tests/replay/run.sh then compares the client's decoder against this one
on the same files.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "tools", "stream"))
from replay import read_records, decode_runs  # noqa: E402

SET_MODE, PALETTE, RECT, FRAME_END, RESET, SCROLL = range(6)


def frames_of(path):
    records, _, _ = read_records(path)
    width = height = 0
    fb = bytearray()
    frames = []
    scrolls = []
    for family, codec, seq, payload in records:
        if family != 1 or not payload:
            continue
        kind = payload[0]
        if kind == SET_MODE:
            w, h, bpp = struct.unpack_from("<HHB", payload, 1)
            if (w, h) != (width, height):
                width, height = w, h
                fb = bytearray(width * height)
        elif kind == SCROLL:
            dy = struct.unpack_from("<b", payload, 1)[0]
            scrolls.append(dy)
            if dy > 0:
                fb[dy * width:] = fb[:(height - dy) * width]
                fb[:dy * width] = b"\x00" * (dy * width)
            elif dy < 0:
                up = -dy
                fb[:(height - up) * width] = fb[up * width:]
                fb[(height - up) * width:] = b"\x00" * (up * width)
        elif kind == RECT:
            x, y, rw, rh = struct.unpack_from("<HHHH", payload, 1)
            if codec == 0:
                data = payload[9:9 + rw * rh]
                rows = [data[r * rw:(r + 1) * rw] for r in range(rh)]
            else:
                rows = decode_runs(payload[9:], rw, rh)
            if rows is None:
                raise SystemExit(f"{path}: bad rect at sequence {seq}")
            for r in range(rh):
                dst = (y + r) * width + x
                fb[dst:dst + rw] = rows[r]
        elif kind == FRAME_END:
            frames.append(bytes(fb))
    return frames, scrolls


def main(argv):
    if len(argv) != 1:
        raise SystemExit(__doc__)
    root = argv[0]
    rows_frames, rows_scrolls = frames_of(os.path.join(root, "sample-pan-rows.bin"))
    scroll_frames, scroll_scrolls = frames_of(os.path.join(root, "sample-pan-scroll.bin"))
    bad = []
    if len(rows_frames) != len(scroll_frames):
        bad.append(f"{len(rows_frames)} frames vs {len(scroll_frames)}")
    for i, (a, b) in enumerate(zip(rows_frames, scroll_frames)):
        if a != b:
            diff = sum(1 for x, y in zip(a, b) if x != y)
            bad.append(f"frame {i} differs in {diff} bytes")
    if rows_scrolls:
        bad.append(f"the row recording carries {len(rows_scrolls)} scroll message(s)")
    if len(scroll_scrolls) < 8:
        bad.append(f"the scroll recording carries only {len(scroll_scrolls)} scroll message(s)")
    if set(scroll_scrolls) != {-1, 1}:
        bad.append(f"scroll steps {sorted(set(scroll_scrolls))}, expected -1 and +1")
    if bad:
        for line in bad:
            print(f"  FAIL {line}")
        raise SystemExit("seam-scroll: FAIL - the two recordings disagree")
    print(f"seam-scroll: OK (pan of 12 pictures, rows vs "
          f"{len(scroll_scrolls)} scroll messages, identical frames)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
