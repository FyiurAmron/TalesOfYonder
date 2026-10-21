#!/usr/bin/env python3
"""Parse DOSBox raw OPL (DRO v2) captures into OPL register writes.

    python3 tools/stream/dro.py summary FILE
    python3 tools/stream/dro.py dump    FILE [--from MS] [--to MS] [--reg R[,R..]] [--limit N]

`summary` prints the capture header, the write/delay counts, the write time
span and a register histogram.  `dump` prints "MS REG=VALUE" lines for a time
window (and/or a register filter), which is what the port's OPL layer is
compared against: the register writes are exactly what the driver wrote to
the FM chip, with DOSBox-X's capture timestamp.

Format: "DBRAWOPL", major/minor (2.0), pair count, millisecond length,
hardware type (0 = OPL2), format, compression, short/long delay codes,
codemap length + table; then pair count times (index byte, value byte).
Delay codes carry "value + 1" ms (short) or "(value + 1) * 256" ms (long);
every other index is an entry in the codemap, with bit 7 selecting the second
register bank (dual OPL2 / OPL3).  DOSBox-X writes a snapshot of the current
register state first, so the opening writes are not all from the song.

The capture keeps a **last-value cache per register**: a write that does not
change a register's value is not in the file at all (both captures here have
zero writes whose value equals the register's previous value).  A comparison
against another write stream has to apply the same filter.
"""

import collections
import struct
import sys

SHORT_MS = 1  # added to a short delay value
LONG_MS = 256  # multiplied into a long delay value


def fail(msg):
    sys.stderr.write("dro: %s\n" % msg)
    raise SystemExit(1)


def load(path):
    d = open(path, "rb").read()
    if len(d) < 26 or d[:8] != b"DBRAWOPL":
        fail("%s is not a DBRAWOPL capture" % path)
    major, minor = struct.unpack_from("<HH", d, 8)
    if (major, minor) != (2, 0):
        fail("unsupported DRO version %d.%d (only 2.0 is decoded)" % (major, minor))
    pairs, ms = struct.unpack_from("<II", d, 12)
    hw, fmt, comp, short, long_, cmlen = d[20:26]
    cmap = d[26 : 26 + cmlen]
    if len(cmap) != cmlen:
        fail("truncated codemap")
    if comp != 0:
        fail("unsupported compression %d" % comp)
    if fmt != 0:
        fail("unsupported data arrangement %d" % fmt)
    data = d[26 + cmlen :]
    if len(data) < pairs * 2:
        sys.stderr.write(
            "dro: warning: truncated capture (%d of %d pairs)\n" % (len(data) // 2, pairs)
        )
    writes = []
    t = 0
    short_n = long_n = 0
    for i in range(0, len(data) - 1, 2):
        code, val = data[i], data[i + 1]
        if code == short:
            t += val + SHORT_MS
            short_n += 1
            continue
        if code == long_:
            t += (val + 1) * LONG_MS
            long_n += 1
            continue
        bank = (code >> 7) & 1
        idx = code & 0x7F
        if idx >= cmlen:
            fail("pair %d: codemap index %d out of range" % (i // 2, idx))
        writes.append((t, bank, cmap[idx], val))
    return {
        "path": path,
        "ms": ms,
        "hardware": hw,
        "pairs": pairs,
        "codemap": cmap,
        "short": short_n,
        "long": long_n,
        "writes": writes,
    }


def key_on(w):
    """True for the register/value writes that actually start a note."""
    _, _, reg, val = w
    if 0xA0 <= reg <= 0xA8:  # frequency low: not the note-on gate
        return False
    if 0xB0 <= reg <= 0xB8:
        return bool(val & 0x20)
    if reg == 0xBD:
        return bool(val & 0x1F)
    return False


def summary(path):
    c = load(path)
    w = c["writes"]
    print("%s: DRO 2.0, hardware %d, %d pairs (%d writes, %d short / %d long delays)"
          % (c["path"], c["hardware"], c["pairs"], len(w), c["short"], c["long"]))
    print("  declared length %d ms; last write at %d ms" % (c["ms"], w[-1][0] if w else 0))
    regs = collections.Counter(r for _, b, r, _ in w if b == 0)
    banks = collections.Counter(b for _, b, _, _ in w)
    print("  banks: %s" % ", ".join("%d x%d" % (b, n) for b, n in sorted(banks.items())))
    top = " ".join("%02X:%d" % (r, n) for r, n in regs.most_common(12))
    print("  registers: %d distinct, top %s" % (len(regs), top))
    ons = [t for t in w if key_on(t)]
    if ons:
        print("  %d note-on writes: first at %d ms, last at %d ms" % (len(ons), ons[0][0], ons[-1][0]))
    return c


def dump(c, t0, t1, regs, limit):
    n = 0
    for t, bank, reg, val in c["writes"]:
        if t < t0 or t > t1:
            continue
        if regs and reg not in regs:
            continue
        print("%7d %s%02X=%02X" % (t, "2:" if bank else "", reg, val))
        n += 1
        if limit and n >= limit:
            break
    print("; %d writes" % n)


def main():
    args = sys.argv[1:]
    if not args or args[0] not in ("summary", "dump"):
        fail("usage: dro.py summary|dump FILE [--from MS] [--to MS] [--reg R[,R..]] [--limit N]")
    cmd, path = args[0], args[1]
    t0, t1, limit = 0, 1 << 30, 0
    regs = set()
    i = 2
    while i < len(args):
        if args[i] == "--from":
            t0 = int(args[i + 1], 0)
        elif args[i] == "--to":
            t1 = int(args[i + 1], 0)
        elif args[i] == "--limit":
            limit = int(args[i + 1], 0)
        elif args[i] == "--reg":
            regs = {int(x, 16) for x in args[i + 1].split(",")}
        else:
            fail("unknown argument %s" % args[i])
        i += 2
    c = summary(path)
    if cmd == "dump":
        dump(c, t0, t1, regs, limit)


if __name__ == "__main__":
    main()
