#!/usr/bin/env python3
"""Check the boot-time save-slot scan against staged saves.

    python3 tests/c/ffscan.py DUMPDIR GAMEDIR

`tests/c/ffscan.sh` boots the game with SAVGAME1/SAVGAME3 staged and dumps
DGROUP. The boot scan (sub_01D84) probes each SAVGAMEd literally: found
slots get flag bit 0x80 plus the file header copied ASCIIZ over the
"- EMPTY -" default (fn_13D7A); missing slots keep flags 0 and the
default text.
"""

from __future__ import annotations

import pathlib
import sys

SLOTS = 0x6CC4
RECORD = 0x1B


def fail(message: str) -> int:
    sys.stderr.write(f"ffscan: {message}\n")
    return 1


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write("usage: ffscan.py DUMPDIR GAMEDIR\n")
        return 2
    dump = pathlib.Path(sys.argv[1])
    game = pathlib.Path(sys.argv[2])
    dgroup = (dump / "dgroup.bin").read_bytes()
    for slot in range(1, 7):
        record = dgroup[SLOTS + (slot - 1) * RECORD:]
        digit = record[0]
        flags = record[1]
        name = bytes(record[2:27])
        staged = game / f"SAVGAME{slot}"
        if digit != ord("0") + slot:
            return fail(f"slot {slot} digit is {digit:02X}")
        if staged.is_file():
            if flags & 0x80 == 0:
                return fail(f"slot {slot} exists but is not marked found")
            header = staged.read_bytes()[:25]
            cut = header.index(b"\x00") + 1
            if name[:cut] != header[:cut]:
                return fail(f"slot {slot} name is {name[:cut]!r}, "
                            f"expected {header[:cut]!r}")
        else:
            if flags & 0x80:
                return fail(f"slot {slot} is missing but marked found")
            if b"- EMPTY -" not in name:
                return fail(f"slot {slot} name is {name!r}, "
                            f"expected the EMPTY default")
    print("ffscan: staged slots found with file headers, "
          "missing slots empty")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
