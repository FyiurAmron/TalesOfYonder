#!/usr/bin/env python3
"""Pin the non-seam part of engine/asm/swreg/src/swreg.asm.

    python3 engine/asm/swreg/tools/seam_pin.py --check     # exit 1 if the pin moved (default)
    python3 engine/asm/swreg/tools/seam_pin.py --update    # refresh engine/asm/swreg/src/swreg.pin
    python3 engine/asm/swreg/tools/seam_pin.py --show      # print the current values

The seam is the client integration surface: the `IFDEF SEAM` blocks in
swreg.asm (hook call sites and the include of engine/asm/swreg/src/seam.asm) plus seam.asm
itself.  Everything else in swreg.asm is the game as disassembled; this pin
covers exactly that part, so an edit to game code cannot ride along with seam
work unnoticed.  The byte gate (engine/asm/swreg/tools/verify.sh) is the second half: it
proves the default build is still identical to the reference image.

Refresh the pin only when the change is intentional, and commit it with the
change:

    python3 engine/asm/swreg/tools/seam_pin.py --update
"""

import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
SRC = os.path.join(ROOT, "engine", "asm", "swreg", "src", "swreg.asm")
PIN = os.path.join(ROOT, "engine", "asm", "swreg", "src", "swreg.pin")

# Guard names that mark seam blocks (STREAM is the M1b spike name, kept so the
# pin could be computed before the seam split and compared after it).
SEAM_NAMES = ("STREAM", "SEAM")

IF_RE = re.compile(r"^\s*(?:ifdef|ifndef|if)\s+([A-Za-z_][A-Za-z_0-9]*)", re.I)
ELSE_RE = re.compile(r"^\s*(?:elseif|else)\b", re.I)
ENDIF_RE = re.compile(r"^\s*endif\b", re.I)


def strip_seam(text):
    """Return the source with every seam-guarded block removed."""
    out = []
    seam_depth = None  # None = outside, else nesting depth inside the block
    for line in text.splitlines(keepends=True):
        if seam_depth is None:
            m = IF_RE.match(line)
            if m and m.group(1).upper() in SEAM_NAMES:
                seam_depth = 0
                continue
            out.append(line)
        else:
            if IF_RE.match(line):
                seam_depth += 1
            elif ENDIF_RE.match(line):
                if seam_depth == 0:
                    seam_depth = None
                else:
                    seam_depth -= 1
    if seam_depth is not None:
        raise SystemExit("seam_pin: unterminated seam block in %s" % SRC)
    return "".join(out)


def compute():
    text = open(SRC, "r", encoding="utf-8", errors="surrogateescape").read()
    stripped = strip_seam(text)
    return {
        "sha256": hashlib.sha256(stripped.encode("utf-8", "surrogateescape")).hexdigest(),
        "bytes": len(stripped.encode("utf-8", "surrogateescape")),
        "lines": stripped.count("\n"),
    }


def read_pin():
    values = {}
    if os.path.exists(PIN):
        for line in open(PIN):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                values[k.strip()] = v.strip()
    return values


def write_pin(values):
    with open(PIN, "w") as f:
        f.write("# non-seam part of engine/asm/swreg/src/swreg.asm - see engine/asm/swreg/tools/seam_pin.py\n")
        f.write("# update with: python3 engine/asm/swreg/tools/seam_pin.py --update\n")
        for k in ("sha256", "bytes", "lines"):
            f.write("%s=%s\n" % (k, values[k]))


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--check"
    cur = compute()
    if mode == "--show":
        for k in ("sha256", "bytes", "lines"):
            print("%s=%s" % (k, cur[k]))
        return 0
    if mode == "--update":
        write_pin(cur)
        print("seam_pin: updated %s" % os.path.relpath(PIN, ROOT))
        for k in ("sha256", "bytes", "lines"):
            print("  %s=%s" % (k, cur[k]))
        return 0
    if mode != "--check":
        sys.stderr.write("usage: seam_pin.py [--check|--update|--show]\n")
        return 2
    pinned = read_pin()
    if not pinned:
        sys.stderr.write("seam_pin: no pin at %s (run --update)\n" % PIN)
        return 1
    if all(pinned.get(k) == str(cur[k]) for k in ("sha256", "bytes", "lines")):
        print("seam_pin: OK  non-seam swreg.asm unchanged (%s)" % cur["sha256"][:16])
        return 0
    sys.stderr.write("seam_pin: engine/asm/swreg/src/swreg.asm changed OUTSIDE the seam blocks\n")
    for k in ("sha256", "bytes", "lines"):
        sys.stderr.write("  %-6s pinned %-64s current %s\n" % (k, pinned.get(k), cur[k]))
    sys.stderr.write("If this is intentional game-source work, refresh the pin with\n"
                     "  python3 engine/asm/swreg/tools/seam_pin.py --update\n"
                     "and commit the pin together with the change.\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
