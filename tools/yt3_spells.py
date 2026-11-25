#!/usr/bin/env python3
"""Dump YT3 WORLD.DAT spell effect descriptions.

YT3 holds the F3 detail EFFECT data twice in the engine's own format:
a (start, count) u16 index plus 39-byte NUL-terminated text records
(the same layout YT2 reads from WORLD 16762Eh/167822h):

  copy A  index 3D4AEFh (108 slots, slot 0 is the (0,0) header)
          text  3D4C9Fh, records 0-61 clean, then truncated by copy B
  copy B  index 3D5631h (B[i] == A[i+1]; slot 107 reads past the chain)
          text  3D57E1h, records 0-353 all clean English

Spell file F reads slot F+1 (the mapping proven against YT2's oracle
frames).  Without the YT3 executable the live chunk choice is unknown;
this tool just renders what each copy holds.

Usage: tools/yt3_spells.py WORLD.DAT [A|B] [FILE...]
"""

from __future__ import annotations

import struct
import sys

COPIES = {
    "A": (0x3D4AEF, 0x3D4C9F),
    "B": (0x3D5631, 0x3D57E1),
}
SPELLS = 0x41B5BF
NFILES = 107
REC = 39


def main(argv: list[str]) -> int:
    if len(argv) < 2 or (len(argv) >= 3 and argv[2] not in COPIES):
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    world = open(argv[1], "rb").read()
    idx, txt = COPIES[argv[2] if len(argv) >= 3 else "B"]
    files = [int(a) for a in argv[3:]] or list(range(NFILES))
    for f in files:
        if not 0 <= f < NFILES:
            print(f"file {f} out of range", file=sys.stderr)
            return 2
        name = world[SPELLS + f * 80:SPELLS + f * 80 + 21]
        name = name.split(b"\x00")[0].decode("ascii").strip()
        start, count = struct.unpack("<HH", world[idx + (f + 1) * 4:][:4])
        print(f"--- file {f} {name} ({start},{count})")
        for r in range(start, start + count):
            raw = world[txt + r * REC:txt + (r + 1) * REC]
            print("    " + raw.split(b"\x00")[0].decode("ascii", "replace"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
