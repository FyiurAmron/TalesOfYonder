#!/usr/bin/env python3
"""Decode the harness's RLE.BIN and compare it with ORG.BIN.

    check.py WORKDIR

Exit 0 only if every row decodes to its source row byte for byte.  The decoder
here is written against docs/protocol.md, independently of both the client and
tools/stream/replay.py, so a format change must be made in all three.
"""
import struct
import sys

ROWS = 6
WIDTH = 640
OPS = {0: "repeat the previous pixel", 1: "repeat a value", 2: "literals"}


def decode(data, width):
    """Return (row, None) or (None, why not)."""
    row = bytearray()
    at = 0
    previous = 0
    have = False
    while len(row) < width:
        if at >= len(data):
            return None, f"truncated at {len(row)} of {width}"
        op = data[at]
        at += 1
        if op == 3:
            break
        if op == 0:
            if not have:
                return None, "repeats the previous pixel before any pixel"
            if at >= len(data):
                return None, "truncated count"
            count = data[at]
            at += 1
            value = previous
        elif op == 1:
            if at + 1 >= len(data):
                return None, "truncated repeat"
            count = data[at]
            value = data[at + 1]
            at += 2
        elif op == 2:
            if at >= len(data):
                return None, "truncated literal count"
            count = data[at]
            at += 1
            if at + count > len(data):
                return None, "literals past the end"
            row += data[at:at + count]
            at += count
            if count:
                previous = row[-1]
                have = True
            if len(row) > width:
                return None, "literals past the row"
            continue
        else:
            return None, f"unknown opcode {op} ({OPS.get(op, '?')})"
        if count == 0:
            return None, "zero count"
        if len(row) + count > width:
            return None, "run past the row"
        row += bytes([value]) * count
        previous = value
        have = True
    if len(row) != width:
        return None, f"row is {len(row)} of {width}"
    return bytes(row), None


def main(argv):
    if len(argv) != 1:
        raise SystemExit(__doc__)
    work = argv[0]
    enc = open(f"{work}/RLE.BIN", "rb").read()
    org = open(f"{work}/ORG.BIN", "rb").read()
    if len(org) != ROWS * WIDTH:
        print(f"seam-rle: ORG.BIN is {len(org)} bytes, expected {ROWS * WIDTH}",
              file=sys.stderr)
        return 2
    at = 0
    failed = 0
    for index in range(ROWS):
        if at + 2 > len(enc):
            print(f"seam-rle: row {index} has no length word", file=sys.stderr)
            return 2
        length = struct.unpack_from("<H", enc, at)[0]
        at += 2
        payload = enc[at:at + length]
        at += length
        if len(payload) != length:
            print(f"seam-rle: row {index} payload is {len(payload)} of {length}",
                  file=sys.stderr)
            return 2
        want = org[index * WIDTH:(index + 1) * WIDTH]
        got, error = decode(payload, WIDTH)
        if error:
            print(f"seam-rle: row {index}: {error} ({length} bytes)", file=sys.stderr)
            failed += 1
        elif got != want:
            first = next(i for i in range(WIDTH) if got[i] != want[i])
            print(f"seam-rle: row {index}: pixel {first} is {got[first]}, "
                  f"want {want[first]}", file=sys.stderr)
            failed += 1
        else:
            print(f"seam-rle: row {index}: OK, {length} bytes for {WIDTH} pixels")
    if at != len(enc):
        print(f"seam-rle: {len(enc) - at} trailing bytes", file=sys.stderr)
        failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
