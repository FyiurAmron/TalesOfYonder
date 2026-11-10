#!/usr/bin/env python3
"""Check the game's own save/load round trip through the disk panel.

    python3 tests/c/saveload.py DUMPDIR GAMEDIR

`tests/c/saveload.sh` runs the M5 opening and then, through the disk panel:
saves the chest-loot state to `SAVGAME1`, walks four steps back and saves the
changed state to `SAVGAME2`, loads `SAVGAME1` back and saves the restored
state to `SAVGAME3`.

`SAVGAME<n>` is the same 77509-byte blob as `CURGAME`, copied from
`DGROUP:944Fh`; the fields this checks are:

    00h  `MITHWARE PARTY` header
    96h  facing (v_94E5)
    98h  party x (v_94E7)
    9Ah  party y (v_94E9)
    1F4h the first party slot (v_9643 name), and the rest of the party state

The end-of-run `--dump` confirms the live DGROUP copy too.
"""

from __future__ import annotations

import pathlib
import struct
import sys

SIZE = 77509
FACE = 0x96
X = 0x98
Y = 0x9A
NAME = 0x1F4
NAME_BYTES = b"CONAN\x00"

# The M5 slice leaves the party at the chest: facing south, (A1h, 22h).
SAVED_FACE = 0x4000
SAVED_X = 0x00A1
SAVED_Y = 0x0022
# Four down-arrow steps move the party north by four units.
WALK = 4

# Live DGROUP at the end of the load run (the M5 settled party state).
LIVE = (("v_94E7", 0x94E7, SAVED_X),
        ("v_94E9", 0x94E9, SAVED_Y),
        ("v_94E5", 0x94E5, SAVED_FACE),
        ("v_5062", 0x5062, 0x0644),
        ("v_5064", 0x5064, 0x0001))


def fail(message: str) -> None:
    sys.stderr.write(f"saveload: {message}\n")
    raise SystemExit(1)


def field16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def party(path: pathlib.Path) -> bytes:
    if not path.is_file():
        fail(f"{path} was not written (wrong slot digit or the menu did not open)")
    data = path.read_bytes()
    if len(data) != SIZE:
        fail(f"{path} is {len(data)} bytes, expected {SIZE}")
    if data[1:15] != b"MITHWARE PARTY":
        fail(f"{path} does not start with the MITHWARE PARTY header")
    if data[NAME:NAME + len(NAME_BYTES)] != NAME_BYTES:
        fail(f"{path} slot 0 name is {data[NAME:NAME + 6]!r}, "
             f"expected {NAME_BYTES!r}")
    return data


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write("usage: saveload.py DUMPDIR GAMEDIR\n")
        return 2
    dump = pathlib.Path(sys.argv[1])
    game = pathlib.Path(sys.argv[2])

    saved = party(game / "SAVGAME1")
    walked = party(game / "SAVGAME2")
    restored = party(game / "SAVGAME3")

    if field16(saved, FACE) != SAVED_FACE or \
            field16(saved, X) != SAVED_X or field16(saved, Y) != SAVED_Y:
        fail("slot 1 saved "
             f"({field16(saved, X):04X},{field16(saved, Y):04X}) facing "
             f"{field16(saved, FACE):04X}, expected "
             f"({SAVED_X:04X},{SAVED_Y:04X}) facing {SAVED_FACE:04X}")
    if field16(walked, X) != SAVED_X or \
            field16(walked, FACE) != SAVED_FACE or \
            field16(walked, Y) != SAVED_Y - WALK:
        fail("slot 2 saved "
             f"({field16(walked, X):04X},{field16(walked, Y):04X}) facing "
             f"{field16(walked, FACE):04X}; the four-step walk did not make it "
             f"into the save")
    if field16(restored, FACE) != SAVED_FACE or \
            field16(restored, X) != SAVED_X or field16(restored, Y) != SAVED_Y:
        fail("slot 3 saved "
             f"({field16(restored, X):04X},{field16(restored, Y):04X}) facing "
             f"{field16(restored, FACE):04X}, expected the slot-1 state "
             f"({SAVED_X:04X},{SAVED_Y:04X}) facing {SAVED_FACE:04X}")
    if restored[NAME:] != saved[NAME:]:
        fail("the loaded party state does not match the save from the first "
             "party record to the end of the blob")
    if walked == saved:
        fail("the walk did not change the saved blob; the mutation is blind")

    dgroup = (dump / "dgroup.bin").read_bytes()
    for name, offset, expected in LIVE:
        value = field16(dgroup, offset)
        if value != expected:
            fail(f"live {name} is {value:04X}, expected {expected:04X}")
    if dgroup[0x9643:0x9643 + len(NAME_BYTES)] != NAME_BYTES:
        fail("live slot 0 name is "
             f"{dgroup[0x9643:0x9643 + 6]!r}, expected {NAME_BYTES!r}")

    print(f"saveload: saved ({SAVED_X:04X},{SAVED_Y:04X}), walked to "
          f"({SAVED_X:04X},{SAVED_Y - WALK:04X}), load restored "
          f"({SAVED_X:04X},{SAVED_Y:04X}); "
          f"{len(saved) - NAME} party bytes match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
