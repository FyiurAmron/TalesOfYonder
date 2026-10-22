#!/usr/bin/env python3
"""Copy the seam's row encoder out of seam.asm for the round-trip harness.

    extract.py engine/asm/swreg/src/seam.asm OUT.inc

The encoder is plain 16-bit code with no includes of its own: it reads the row
through DS:SI and writes the runs through DS:DI, using only its six state
variables.  The harness supplies those as absolute equates, so the extracted
text assembles as-is inside a .COM.
"""
import sys

START = "sub_SeamEncodeRow:"
END = "; sub_SeamSend - frame and transmit one message."


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    lines = open(argv[0]).read().splitlines()
    start = next((i for i, l in enumerate(lines) if l.startswith(START)), None)
    end = next((i for i, l in enumerate(lines) if l.startswith(END)), None)
    if start is None or end is None or end <= start:
        raise SystemExit("extract: the encoder block is not in " + argv[0])
    while end > start and lines[end].startswith(";"):
        end -= 1
    with open(argv[1], "w") as out:
        out.write("\n".join(lines[start:end]).rstrip() + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
