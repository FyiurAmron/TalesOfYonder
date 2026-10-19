#!/usr/bin/env python3
"""Check the percussion OPL writes of an API run against the reference driver.

    python3 engine/asm/sbfmdrv/test/perc.py DRO RESULT.TXT

`sh engine/asm/sbfmdrv/test/run.sh api` with `API_CAP=dro` wraps TEST.COM in DOSBox-X's
`DX-CAPTURE /O`, which records every FM register write of the run.  This script
slices the percussion section (from the rhythm-on write `BDh = E0h` to the
rhythm-off `BDh = C0h`) and replays what `sub_0FD5`/`sub_0EB8` must have
written:

    per hit       40h + level   KSL/TL = 3Fh - velocityTable[(velocity+4)>>3]
                  A0h + channel the note's frequency word, low byte
                  B0h + channel its high byte (no key-on: rhythm mode)
                  BDh           the rhythm shadow with the hit's bit set
    per release   BDh           the shadow with that bit cleared

The velocity table is read from the reference image (CS:0A1Ch; a .COM file
offset is the address minus 100h), the note words and velocities come from the
crafted stream in test.asm and from the DRUMS line of RESULT.TXT.

DOSBox-X's DRO writer keeps a last-value cache per register and drops writes
that do not change a register, so the replay applies the same filter.  That is
exactly why the cymbal's `B0h = 09h` write is absent: the tom wrote it one hit
earlier.  The drum registers come from docs/sbfmdrv.md 4.4 (levels
10h,14h,12h,15h,11h for channels 0Bh..0Fh, +3 for the bass drum; OPL channels
6,7,8,8,7).

`SBFMDRV_EXE` overrides the reference path.
"""

import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
REF = os.environ.get("SBFMDRV_EXE", os.path.join(ROOT, "_ref", "orig-bin", "SBFMDRV.COM"))
VEL_TABLE = 0x0A1C - 0x100  # CS:0A1Ch in a .COM image

# The crafted stream in test.asm (stream_perc): one hit and one release per
# drum, in channel order 0Bh..0Fh (bass drum, snare, tom, cymbal, hi-hat).
VELOCITIES = (0x64, 0x50, 0x48, 0x40, 0x3C)
MASKS = (0x10, 0x08, 0x04, 0x02, 0x01)
LEVELS = (0x10, 0x14, 0x12, 0x15, 0x11)
OPL_CHANNELS = (6, 7, 8, 8, 7)
WORDS = ("6", "7", "8", "9", "A")


def fail(msg):
    sys.stderr.write("perc: %s\n" % msg)
    raise SystemExit(1)


def velocity_table():
    data = open(REF, "rb").read()
    if len(data) < VEL_TABLE + 64:
        fail("%s is too small for the velocity table" % REF)
    return struct.unpack_from("<32H", data, VEL_TABLE)


def drums_words(path):
    for line in open(path, "r", errors="replace"):
        if not line.startswith("DRUMS"):
            continue
        found = {
            m.group(1): int(m.group(2), 16)
            for m in re.finditer(r"W([0-9A])=([0-9A-F]{4})", line)
        }
        if all(k in found for k in WORDS):
            return [found[k] for k in WORDS]
    fail("no DRUMS line with W6..WA in %s" % path)


def replay(table, words, initial):
    """Expected percussion writes, filtered like the DRO writer."""
    out = []
    last = dict(initial)

    def w(reg, val):
        if last.get(reg) != val:
            out.append((reg, val))
            last[reg] = val

    shadow = 0xE0
    w(0xBD, shadow)
    for i in range(5):
        tl = 0x3F - min(table[(VELOCITIES[i] + 4) >> 3], 0x3F)
        w(0x40 + LEVELS[i] + (3 if i == 0 else 0), tl)
        w(0xA0 + OPL_CHANNELS[i], words[i] & 0xFF)
        w(0xB0 + OPL_CHANNELS[i], (words[i] >> 8) & 0xFF)
        shadow |= MASKS[i]
        w(0xBD, shadow)
    for i in range(5):
        shadow &= ~MASKS[i] & 0xFF
        w(0xBD, shadow)
    w(0xBD, 0xC0)
    return out


def main():
    if len(sys.argv) != 3:
        fail("usage: perc.py DRO RESULT.TXT")
    sys.path.insert(0, os.path.join(ROOT, "tools", "stream"))
    import dro

    capt = dro.load(sys.argv[1])
    writes = capt["writes"]
    start = next((i for i, t in enumerate(writes) if (t[2], t[3]) == (0xBD, 0xE0)), None)
    if start is None:
        fail("no rhythm-on write (BDh = E0h) in %s" % sys.argv[1])
    end = next(
        (i for i, t in enumerate(writes) if i > start and (t[2], t[3]) == (0xBD, 0xC0)),
        None,
    )
    if end is None:
        fail("no rhythm-off write (BDh = C0h) after the rhythm-on")
    got = [(t[2], t[3]) for t in writes[start : end + 1]]

    # register state before the section (the DRO's own value cache)
    initial = {}
    for t in writes[:start]:
        initial[t[2]] = t[3]

    want = replay(velocity_table(), drums_words(sys.argv[2]), initial)
    print(
        "PERC.DRO: %d writes from BD=%02X to BD=%02X (last write at %d ms)"
        % (len(got), writes[start][3], writes[end][3], writes[end][0])
    )
    if got == want:
        print("PERC.DRO: OK  %d/%d writes match the decoded percussion path" % (len(got), len(want)))
        return 0
    n = min(len(got), len(want))
    for i in range(n):
        if got[i] != want[i]:
            print("  first difference at write %d:" % i)
            print("    capture:   %02X=%02X" % got[i])
            print("    expected:  %02X=%02X" % want[i])
            break
    if len(got) != len(want):
        print("  capture has %d writes, expected %d" % (len(got), len(want)))
    print("PERC.DRO: MISMATCH")
    return 1


if __name__ == "__main__":
    sys.exit(main() or 0)
