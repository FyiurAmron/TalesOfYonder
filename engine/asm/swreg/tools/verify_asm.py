#!/usr/bin/env python3
"""Instruction-level byte diff between a rebuilt SWREG.EXE and the reference.

Usage:
    python3 asm/tools/verify_asm.py [candidate.exe] [--listing swreg.lst] [--max N]

Default reference: ../../_ref/orig-bin/SWREG.EXE resolved from this script's location
(the repo layout), or $SWREG_EXE if set. Default candidate: ./SWREG.EXE.

The reference image is decoded with the local trace/model tooling, so differences
are attributed to instructions (address + expected/got bytes + mnemonic) instead
of raw offsets. Differences outside decoded code are grouped into byte ranges.

Exit codes: 0 = identical, 1 = differences found, 2 = usage/IO error.
"""
import os
import re
import struct
import sys

MAXDEFAULT = 60


def die(msg):
    print("verify_asm: " + msg, file=sys.stderr)
    raise SystemExit(2)


def main(argv):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_ref = os.path.normpath(
        os.path.join(script_dir, os.pardir, os.pardir, "_ref", "orig-bin", "SWREG.EXE")
    )
    cand = None
    listing_path = None
    maxshow = MAXDEFAULT

    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--listing" and i + 1 < len(argv):
            listing_path = argv[i + 1]
            i += 2
        elif a == "--max" and i + 1 < len(argv):
            try:
                maxshow = max(1, int(argv[i + 1]))
            except ValueError:
                die("bad --max value: %r" % argv[i + 1])
            i += 2
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        elif a.startswith("-"):
            die("unknown option: " + a)
        else:
            if cand is not None:
                die("unexpected extra argument: " + a)
            cand = a
            i += 1

    ref = os.environ.get("SWREG_EXE") or repo_ref
    cand = cand or "SWREG.EXE"
    for p in (ref, cand):
        if not os.path.isfile(p):
            die("file not found: " + p)

    R = open(ref, "rb").read()
    C = open(cand, "rb").read()

    print("reference: %s (%d bytes)" % (ref, len(R)))
    print("candidate: %s (%d bytes)" % (cand, len(C)))

    if R == C:
        print("IDENTICAL")
        return 0

    if len(R) != len(C):
        print("NOTE: sizes differ by %+d bytes" % (len(C) - len(R)))

    hdr = struct.unpack("<H", R[8:10])[0] * 16
    n = min(len(R), len(C))
    diffs = [k for k in range(n) if R[k] != C[k]]
    print("differing bytes: %d%s" % (len(diffs), " (common prefix)" if len(R) != len(C) else ""))

    # optional UASM listing -> source text per address
    listing = {}
    if listing_path:
        if not os.path.isfile(listing_path):
            die("listing not found: " + listing_path)
        for line in open(listing_path, errors="replace"):
            m = re.match(r"^([0-9A-F]{8})  ([0-9A-F]*) +(.*)$", line.rstrip())
            if m:
                listing[int(m.group(1), 16)] = m.group(3).strip()

    # decode the reference image (needs SWREG_EXE set before importing the tools)
    os.environ["SWREG_EXE"] = os.path.abspath(ref)
    sys.path.insert(0, script_dir)
    from trace import run  # noqa: E402

    cov, _bad = run()
    starts = sorted(cov)

    dset = set(diffs)
    hdr_diffs = [k for k in diffs if k < hdr]

    bad_ins = []
    covered = set()
    for a in starts:
        ins = cov[a]
        exp = R[hdr + a: hdr + a + ins.size]
        got = C[hdr + a: hdr + a + ins.size]
        covered.update(range(hdr + a, hdr + a + ins.size))
        if exp != got:
            bad_ins.append((a, exp, got, ins))

    print()
    if hdr_diffs:
        print("header/relocation region: %d differing byte(s), first at file offset %#06x"
              % (len(hdr_diffs), hdr_diffs[0]))
    else:
        print("header/relocation region: clean")

    if bad_ins:
        print("code: %d differing instruction(s)" % len(bad_ins))
        for a, exp, got, ins in bad_ins[:maxshow]:
            txt = listing.get(a) or ("%s %s" % (ins.mnemonic, ins.op_str))
            print("  image:%05X  expected %s | got %s   ; %s"
                  % (a, exp.hex(" "), got.hex(" "), txt))
        if len(bad_ins) > maxshow:
            print("  ... %d more instruction(s)" % (len(bad_ins) - maxshow))
    else:
        print("code: clean")

    rest = sorted(dset - covered - set(hdr_diffs))
    ranges = []
    for k in rest:
        if ranges and k == ranges[-1][1]:
            ranges[-1][1] = k + 1
        else:
            ranges.append([k, k + 1])
    if ranges:
        total = sum(b - a for a, b in ranges)
        print("data/other: %d range(s), %d byte(s)" % (len(ranges), total))
        for a, b in ranges[:maxshow]:
            ctx = min(16, b - a)
            print("  file:%06X image:%05X  expected %s | got %s"
                  % (a, a - hdr, R[a:a + ctx].hex(" "), C[a:a + ctx].hex(" ")))
        if len(ranges) > maxshow:
            print("  ... %d more range(s)" % (len(ranges) - maxshow))
    else:
        print("data/other: clean")

    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
