#!/usr/bin/env python3
"""Copy the seam's scroll detector out of seam.asm for the harness.

    extract.py engine/asm/swreg/src/seam.asm OUT.inc

The routines are plain 16-bit code over two row-checksum tables that live at
fixed offsets inside the seam's DOS block (SEAM_SHADOW, SEAM_CURTAB).  A .COM
has the same flat segment, so the harness only has to place the tables at the
same offsets.  The equates travel with the code so a layout change cannot make
the harness test a stale copy.
"""
import sys

EQUATES = ("SEAM_SHADOW ", "SEAM_CURTAB ", "SSC_MAX_STEP ", "SSC_MIN_CHANGED ",
           "SSC_MIN_MATCH ")
VARIABLES = ("seam_changed ", "seam_scrbest ", "seam_scrbestdy ", "seam_scrstep ")
START = "sub_SeamScrollCheck:"
END = "; sub_SeamEnterVbe - allocate and clear the VBE shadow, send set_mode."


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    lines = open(argv[0]).read().splitlines()

    def find(prefix, start=0):
        for i in range(start, len(lines)):
            if lines[i].startswith(prefix):
                return i
        raise SystemExit(f"extract: no line starts with {prefix!r}")

    out = ["; generated from engine/asm/swreg/src/seam.asm - do not edit"]
    for name in EQUATES:
        out.append(lines[find(name)].split(";")[0].rstrip())
    for name in VARIABLES:
        label = name.strip()
        out.append(f"{label} dw 0")
    out.append("")

    start = find(START)
    end = find(END, start)
    while end > start and (lines[end].startswith(";") or not lines[end].strip()):
        end -= 1
    out.append("\n".join(lines[start:end + 1]).rstrip())
    out.append("")

    with open(argv[1], "w") as f:
        f.write("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
