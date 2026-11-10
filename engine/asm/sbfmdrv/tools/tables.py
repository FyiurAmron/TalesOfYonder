#!/usr/bin/env python3
"""Emit engine/c/src/game/sound_tables.h from the reference SBFMDRV.COM.

    python3 engine/asm/sbfmdrv/tools/tables.py [--check] [--out PATH]

The C engine's resident emulation installs the driver's own instrument,
percussion and pitch tables at their resident CS offsets so the game's scanner
and the music blocks behave exactly as they do with the real driver
(docs/sbfmdrv.md 4.4).  This tool slices those tables out of
`_ref/orig-bin/SBFMDRV.COM` and writes the generated header.  `--check`
compares the checked-in file instead of writing it and exits non-zero when it
is stale.

The COM image loads at CS:0100h, so CS offset `cs` lives at file offset
`cs - 100h`.
"""
import argparse
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
REF = os.environ.get("SBFMDRV_EXE", os.path.join(ROOT, "_ref", "orig-bin", "SBFMDRV.COM"))
HEADER = os.path.join(ROOT, "engine", "c", "src", "game", "sound_tables.h")
MD5 = "0f679017d543b0947fc5a33456ba38a7"

# name, CS offset, byte length, C type, values per line, formatter
TABLES = [
    ("yt2_sound_default_instruments", 0x0159, 256, "uint8_t", 16, "0x%02X"),
    ("yt2_sound_operator_base", 0x02CE, 11, "uint8_t", 11, "0x%02X"),
    ("yt2_sound_default_patch", 0x02D7, 11, "uint8_t", 11, "0x%02X"),
    ("yt2_sound_drum_levels", 0x02E2, 5, "uint8_t", 5, "0x%02X"),
    ("yt2_sound_drum_bits", 0x02E7, 5, "uint8_t", 5, "0x%02X"),
    ("yt2_sound_drum_offsets", 0x02EC, 5, "uint8_t", 5, "0x%02X"),
    ("yt2_sound_note_map", 0x039C, 128, "uint8_t", 16, "0x%02X"),
    ("yt2_sound_fnum", 0x041C, 1536, "uint16_t", 16, "0x%04X"),
    ("yt2_sound_velocity", 0x0A1C, 32, "int16_t", 8, "(int16_t)0x%04X"),
]


def fail(message):
    sys.stderr.write("tables: %s\n" % message)
    raise SystemExit(1)


def slices(image):
    """Yield (table, values) with the image sliced at the documented offsets."""
    for name, offset, size, ctype, per_line, fmt in TABLES:
        start = offset - 0x100
        raw = image[start:start + size]
        if len(raw) != size:
            fail("%s (%04Xh) runs past the end of the reference" % (name, offset))
        if ctype == "uint8_t":
            values = list(raw)
        else:
            values = [raw[i] | (raw[i + 1] << 8) for i in range(0, size, 2)]
        yield (name, offset, ctype, per_line, fmt), values


def render(image):
    lines = []
    lines.append("/* Generated from the reference SBFMDRV.COM resident data (CS offsets).")
    lines.append(" * Regenerate with: python3 engine/asm/sbfmdrv/tools/tables.py")
    lines.append(" * The arrays are the default instrument table, operator bases, default")
    lines.append(" * patch, percussion tables, note map, F-number table and velocity curve")
    lines.append(" * at CS:0159h..0A3Bh (docs/sbfmdrv.md 4.4).  Do not edit by hand. */")
    lines.append("")
    lines.append("#ifndef YT2_SOUND_TABLES_H")
    lines.append("#define YT2_SOUND_TABLES_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    for (name, offset, ctype, per_line, fmt), values in slices(image):
        lines.append("")
        lines.append("static const %s %s[%d] = {" % (ctype, name, len(values)))
        for start in range(0, len(values), per_line):
            chunk = values[start:start + per_line]
            lines.append("    " + ", ".join(fmt % value for value in chunk) + ",")
        lines.append("};")
    lines.append("")
    lines.append("#endif /* YT2_SOUND_TABLES_H */")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="compare the checked-in header instead of writing it")
    parser.add_argument("--out", default=HEADER)
    args = parser.parse_args()

    image = open(REF, "rb").read()
    digest = hashlib.md5(image).hexdigest()
    if digest != MD5:
        fail("reference md5 %s, expected %s" % (digest, MD5))
    text = render(image)

    if args.check:
        try:
            current = open(args.out).read()
        except OSError as error:
            fail("cannot read %s: %s" % (args.out, error))
        if current != text:
            fail("%s is stale; regenerate with this tool" % args.out)
        print("tables: %s matches the reference" % args.out)
        return 0

    with open(args.out, "w") as stream:
        stream.write(text)
    print("tables: wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
