#!/usr/bin/env python3
"""Copy the seam's presentation-budget gate out of seam.asm for the harness.

    extract.py engine/asm/swreg/src/seam.asm OUT.inc

`sub_SeamTickGate` allows a pass until the current BIOS tick's budget is used
up, and a forced pass ignores the budget.  A .COM has the same flat segment as
the seam, so the harness can call it unchanged; the constant and the three
variables it uses travel with it, so a rename or a tuning change cannot make
the harness test a stale copy.
"""
import sys

VARIABLES = ("seam_forcepass ", "seam_tick ", "seam_tickpass ")
START = "SEAM_PASSES_PER_TICK equ"
END = "; fn_SeamPresent - emit changed screen rows and close the frame."


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
    for name in VARIABLES:
        out.append(f"{name.strip()} dw 0")
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
