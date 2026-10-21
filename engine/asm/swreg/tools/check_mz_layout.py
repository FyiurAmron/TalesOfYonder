#!/usr/bin/env python3
"""Check the data-segment invariants of a seam build.

    python3 engine/asm/swreg/tools/check_mz_layout.py EXE

The game uses hard-coded DS offsets.  S1DEA's content must keep its original
paragraph prefix (7 in the file) and the 'EMMXXXX0' name must stay 0xC99 bytes
into it, or the EMS init aborts with "Minimum of 1MB Expanded RAM must be
available.".  Hook sites shift the code segment, so the pad at the end of
engine/asm/swreg/src/seam.asm has to compensate exactly; this script fails loudly when a
seam edit moves the data segment.
"""

import os
import sys

S1DEA_SIG = bytes.fromhex("00CE6EAE00D66EC400B66E8200BE6E98")
EMM_NAME = b"EMMXXXX0"
EMM_DELTA = 0xC99


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: check_mz_layout.py EXE")
    path = sys.argv[1]
    data = open(path, "rb").read()
    start = data.find(S1DEA_SIG)
    name = data.find(EMM_NAME)
    if start < 0:
        sys.exit("%s: S1DEA signature not found" % path)
    if name < 0:
        sys.exit("%s: 'EMMXXXX0' not found" % path)
    prefix = start % 16
    delta = name - start
    if prefix != 7 or delta != EMM_DELTA:
        sys.exit("%s: data segment drifted (prefix %d, EMMXXXX0 delta %#x); "
                 "the pad at the end of engine/asm/swreg/src/seam.asm must make the hook "
                 "calls measure a multiple of 16 bytes" % (path, prefix, delta))
    print("%s: S1DEA prefix 7, EMMXXXX0 delta 0xC99" % os.path.basename(path))


if __name__ == "__main__":
    main()
