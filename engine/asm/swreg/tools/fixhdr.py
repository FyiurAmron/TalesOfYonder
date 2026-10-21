# Post-link MZ fix-ups.
#
# 0x1C: the reference sets this byte to 01 (a linker/loader quirk the port
#       reproduces; see docs/disassembly.md).
# 0x0A: eMinAlloc.  The reference asks for 33 paragraphs above the image and
#       the game relies on them (with 0 it writes into the MCB chain).  MASM
#       LINK derives the value from the stack segment; UASM writes 0 when the
#       stack is not the last segment, which the seam build needs so the game's
#       SS stays put.  Pinning the value keeps both builds identical.
import struct
import sys

with open(sys.argv[1], "r+b") as f:
    f.seek(0x1C)
    f.write(b"\x01")
    f.seek(0x0A)
    f.write(struct.pack("<H", 33))
