#!/usr/bin/env python3
"""Byte-verify build/sbfmdrv/SBFMDRV.COM against the reference image.

    SBFMDRV_EXE=<path> python3 engine/asm/sbfmdrv/tools/verify.py [built.com]

Prints "SBFMDRV.COM: OK" and "IDENTICAL" on success; on failure prints the
first differing offsets and exits non-zero.
"""
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
REF = os.environ.get("SBFMDRV_EXE", os.path.join(ROOT, "_ref", "orig-bin", "SBFMDRV.COM"))
BUILT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    ROOT, "build", "driver", "SBFMDRV.COM"
)


def main():
    if not os.path.exists(BUILT):
        sys.stderr.write("verify: missing %s (run engine/asm/sbfmdrv/tools/build.sh)\n" % BUILT)
        return 1
    ref = open(REF, "rb").read()
    got = open(BUILT, "rb").read()
    if got == ref:
        print("SBFMDRV.COM: OK")
        print(
            "IDENTICAL  md5 %s  size %d"
            % (hashlib.md5(got).hexdigest(), len(got))
        )
        return 0
    print(
        "SBFMDRV.COM: MISMATCH  built %d bytes (md5 %s), reference %d bytes (md5 %s)"
        % (len(got), hashlib.md5(got).hexdigest(), len(ref), hashlib.md5(ref).hexdigest())
    )
    shown = 0
    for i in range(min(len(got), len(ref))):
        if got[i] != ref[i]:
            print("  %04X: built %02X, reference %02X" % (0x100 + i, got[i], ref[i]))
            shown += 1
            if shown == 20:
                print("  ...")
                break
    if len(got) != len(ref):
        print("  length differs by %d byte(s)" % (len(got) - len(ref)))
    return 1


if __name__ == "__main__":
    sys.exit(main())
