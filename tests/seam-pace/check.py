#!/usr/bin/env python3
"""Check the seam's presentation budget against its own record.

    check.py WORKDIR          (reads WORKDIR/PACE.BIN, written by pacetest.asm)

The budget is a policy, not a measurement, so the test pins the policy rather
than a clock: up to SEAM_PASSES_PER_TICK passes in one BIOS tick, a forced pass
ignores the budget and clears its flag, and a new tick starts over.  A wrong
clock read or a broken cap shows up as the group's pattern, and the earlier PIT
attempt (which presented nothing at all in a tight loop) would fail here at the
first line.
"""
import struct
import sys

CALLS = 5


def main(argv):
    if len(argv) != 1:
        raise SystemExit(__doc__)
    work = argv[0]
    data = open(f"{work}/PACE.BIN", "rb").read()
    if len(data) != 16:
        raise SystemExit(f"pace: FAIL - PACE.BIN is {len(data)} bytes, not 16")

    passes = struct.unpack_from("<H", data, 0)[0]
    group = list(data[2:7])
    forced = data[7]
    forceleft = struct.unpack_from("<H", data, 8)[0]
    tick0 = struct.unpack_from("<H", data, 10)[0]
    tick1 = struct.unpack_from("<H", data, 12)[0]
    after = data[14]
    tickpass = data[15]

    problems = []
    if not 1 <= passes <= 8:
        problems.append(f"budget {passes} is outside 1..8")
    want = [1] * min(passes, CALLS) + [0] * max(0, CALLS - passes)
    if group != want:
        problems.append(f"five calls in one tick gave {group}, wanted {want}")
    if forced != 1:
        problems.append("a forced pass was denied by an exhausted budget")
    if forceleft != 0:
        problems.append(f"the force flag survived the pass (0x{forceleft:04x})")
    if tick1 == tick0:
        problems.append("the harness never reached the next tick")
    if after != 1:
        problems.append("the first call in the next tick was denied")
    if tickpass != 1:
        problems.append(f"the new tick's pass count is {tickpass}, wanted 1")

    if problems:
        print("pace: FAIL", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        print(f"  record: budget={passes} group={group} forced={forced} "
              f"forceleft={forceleft} ticks={tick0}->{tick1} after={after} "
              f"tickpass={tickpass}", file=sys.stderr)
        return 1
    print(f"pace: OK ({passes} passes per tick: {group}, forced pass runs, "
          f"tick {tick0} -> {tick1} starts over)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
