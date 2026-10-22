#!/usr/bin/env python3
"""Check the seam's scroll detector against an independent model of the spec.

    check.py WORKDIR

scrtest.asm runs the detector over the cases its table names and writes one
205-byte record per case: the message it queued (0 = none, 5 = scroll), the
step, the number of rows the row pass would send, and which ones.  This file
rebuilds the same tables, applies docs/protocol.md's rules (score every step
+-1..+-8, keep a step only when it explains more rows than no shift and at
least 100 of them, then shift the client's table and mark what scrolled in),
and requires the two to agree row for row.
"""
import os
import struct
import sys

MAX_STEP = 8
MIN_CHANGED = 32
MIN_MATCH = 100
ROWS = 200


def base_row(y):
    return (0x1000 + 3 * y) & 0xFFFF, (0x2000 + 5 * y) & 0xFFFF


def build(mode, step, anim):
    """The two tables, exactly as scrtest.asm builds them."""
    cur = [base_row(y) for y in range(ROWS)]
    if mode == 0:
        old = []
        for y in range(ROWS):
            src = y + step
            old.append(base_row(src) if 0 <= src < ROWS else (0xDEAD, 0xBEEF))
    elif mode == 1:
        old = [(~s & 0xFFFF, ~r & 0xFFFF) for s, r in cur]
    else:
        old = list(cur)
        cur[0] = (0x1111, 0x2222)
        if mode == 3:
            for y in range(1, 40):
                cur[y] = (0x1111, 0x2222)
    if anim != 0xFF:
        cur[anim] = (0xDEAD, 0xBEEF)
    return cur, old


def explain(cur, old, step):
    if step > 0:
        return sum(1 for y in range(step, ROWS) if cur[y] == old[y - step])
    if step < 0:
        return sum(1 for y in range(0, ROWS + step) if cur[y] == old[y - step])
    return sum(1 for y in range(ROWS) if cur[y] == old[y])


def model(mode, step, anim):
    """What the seam must do: (kind, step, rows-sent set)."""
    cur, old = build(mode, step, anim)
    changed = sum(1 for y in range(ROWS) if cur[y] != old[y])
    if changed >= MIN_CHANGED:
        best = explain(cur, old, 0)
        bestdy = 0
        for s in range(1, MAX_STEP + 1):
            for dy in (s, -s):
                n = explain(cur, old, dy)
                if n > best:
                    best, bestdy = n, dy
        if bestdy != 0 and best >= MIN_MATCH:
            if bestdy > 0:
                for y in range(ROWS - 1, bestdy - 1, -1):
                    old[y] = old[y - bestdy]
                for y in range(bestdy):
                    old[y] = None
            else:
                for y in range(0, ROWS + bestdy):
                    old[y] = old[y - bestdy]
                for y in range(ROWS + bestdy, ROWS):
                    old[y] = None
            sent = {y for y in range(ROWS) if cur[y] != old[y]}
            return 5, bestdy, sent
    return 0, 0, {y for y in range(ROWS) if cur[y] != old[y]}


CASES = [  # the table in scrtest.asm
    (0, -1, 0xFF),
    (0, -3, 0xFF),
    (0, 2, 0xFF),
    (0, -5, 64),
    (0, 8, 0xFF),
    (0, -12, 0xFF),
    (0, -2, 120),
    (1, 0, 0xFF),
    (2, 0, 0xFF),
    (3, 0, 0xFF),
]


def main(argv):
    if len(argv) != 1:
        raise SystemExit(__doc__)
    work = argv[0]
    com = os.path.join(work, "scrtest.com")
    if os.path.getsize(com) > 0x0940 - 0x100:
        raise SystemExit(f"scrtest.com is {os.path.getsize(com)} bytes: the code "
                         f"would overlap the tables at 0940h")
    data = open(os.path.join(work, "SCR.BIN"), "rb").read()
    size = 205
    if len(data) != size * len(CASES):
        raise SystemExit(f"SCR.BIN is {len(data)} bytes, expected {size * len(CASES)}")
    bad = 0
    for n, (mode, step, anim) in enumerate(CASES):
        kind, dy, rows = struct.unpack_from("<bhH", data, n * size)
        bitmap = data[n * size + 5:(n + 1) * size]
        want_kind, want_dy, want_rows = model(mode, step, anim)
        got_rows = {y for y in range(ROWS) if bitmap[y]}
        problems = []
        if kind != want_kind:
            problems.append(f"message {kind}, expected {want_kind}")
        if dy != want_dy:
            problems.append(f"step {dy:+d}, expected {want_dy:+d}")
        if got_rows != want_rows:
            extra = sorted(got_rows - want_rows)[:6]
            missing = sorted(want_rows - got_rows)[:6]
            problems.append(f"rows sent {len(got_rows)}, expected {len(want_rows)}"
                            f" (extra {extra}, missing {missing})")
        tag = f"case {n} (mode {mode}, step {step:+d}, anim {anim})" 
        if problems:
            bad += 1
            print(f"  FAIL {tag}: " + "; ".join(problems))
        else:
            print(f"  ok   {tag}: " +
                  (f"scroll({want_dy:+d})" if want_kind == 5 else "rows only") +
                  f", {len(want_rows)} row(s) sent" +
                  (f" ({min(want_rows)}..{max(want_rows)})" if want_rows else ""))
    if bad:
        raise SystemExit(f"seam-scroll: FAIL - {bad} of {len(CASES)} cases disagree")
    print(f"seam-scroll: OK ({len(CASES)} cases)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
