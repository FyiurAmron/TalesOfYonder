#!/usr/bin/env python3
"""Check the transliterated game's presented frames against M3 checkpoints.

    python3 tests/c/game_frames.py PRESENT.txt tests/c/game_checkpoints.txt
    python3 tests/c/game_frames.py PRESENT.txt tests/c/game_checkpoints.txt \
        --dump build/runs/c/<stamp>/checkpoint

The present log comes from `yt2game --present-log`: one line per distinct
frame, `<frame> <tick> <fnv-fb> <fnv-palette>`.  The checkpoint file lists the
scenes the original displayed in that order with the same hashes.

The checker requires every checkpoint pair to appear in order and to stay on
screen for `--min-hold` engine ticks before the next change (the last one may
run to the end of the log).  With `--dump` it also verifies that the raw
framebuffer/palette dump at the end of the run hashes to the final checkpoint
and that the settled game globals match the DOSBox-X save states.
"""

import argparse
import struct
import sys

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3

# Settled globals at the attract checkpoints, from the DOSBox-X save states:
# v_94F1 (scene id), v_94D7 (game speed), v_50F6 (tick service flags).
GLOBALS = (("v_94F1", 0x94F1, 0x01E0),
           ("v_94D7", 0x94D7, 0x0005),
           ("v_50F6", 0x50F6, 0x8000))


def fnv1a64(data, value=FNV_OFFSET):
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def read_checkpoints(path):
    items = []
    for number, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 3:
            raise SystemExit(f"game: bad checkpoint line {number}: {line!r}")
        items.append((parts[0], parts[1].lower(), parts[2].lower()))
    if not items:
        raise SystemExit(f"game: no checkpoints in {path}")
    return items


def read_log(path):
    rows = []
    for number, line in enumerate(open(path), 1):
        parts = line.split()
        if len(parts) != 4:
            raise SystemExit(f"game: bad present line {number}: {line!r}")
        rows.append((int(parts[0]), int(parts[1]),
                     parts[2].lower(), parts[3].lower()))
    if not rows:
        raise SystemExit(f"game: no presented frames in {path}")
    return rows


def check_sequence(rows, checkpoints, min_hold):
    cursor = 0
    matched = []
    for name, fb, pal in checkpoints:
        found = None
        for index in range(cursor, len(rows)):
            if rows[index][2] == fb and rows[index][3] == pal:
                found = index
                break
        if found is None:
            same_fb = [row for row in rows[cursor:]
                       if row[2] == fb]
            detail = ""
            if same_fb:
                palettes = sorted({row[3][:8] for row in same_fb})
                detail = (f" (frame {fb[:8]} appears with palette "
                          f"{', '.join(palettes[:4])})")
            raise SystemExit(f"game: checkpoint {name} missing after frame "
                             f"{rows[cursor][0] if cursor < len(rows) else '?'}"
                             f"{detail}")
        hold = None
        if found + 1 < len(rows):
            hold = rows[found + 1][1] - rows[found][1]
            if hold < min_hold:
                raise SystemExit(
                    f"game: checkpoint {name} at frame {rows[found][0]} "
                    f"(tick {rows[found][1]}) lasts only {hold} ticks "
                    f"(need {min_hold})")
        matched.append((name, rows[found], hold))
        cursor = found + 1
    return matched


def check_dump(directory, last, matched):
    with open(f"{directory}/video.bin", "rb") as stream:
        video = fnv1a64(stream.read())
    with open(f"{directory}/palette.bin", "rb") as stream:
        palette = fnv1a64(stream.read())
    if (f"{video:016x}", f"{palette:016x}") != (last[2], last[3]):
        raise SystemExit(
            f"game: end dump is {video:016x}/{palette:016x}, expected "
            f"{last[2]}/{last[3]} ({matched[-1][0]})")
    with open(f"{directory}/dgroup.bin", "rb") as stream:
        dgroup = stream.read()
    for name, offset, expected in GLOBALS:
        value = struct.unpack_from("<H", dgroup, offset)[0]
        if value != expected:
            raise SystemExit(f"game: {name} is {value:04X}, expected "
                             f"{expected:04X}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("checkpoints")
    ap.add_argument("--min-hold", type=int, default=60,
                    help="ticks a checkpoint must stay on screen (default 60)")
    ap.add_argument("--dump", help="checkpoint dump directory from --dump")
    args = ap.parse_args()

    rows = read_log(args.log)
    checkpoints = read_checkpoints(args.checkpoints)
    matched = check_sequence(rows, checkpoints, args.min_hold)
    for name, row, hold in matched:
        held = "to the end" if hold is None else f"{hold} ticks"
        print(f"game: {name:<14} frame {row[0]:>4} tick {row[1]:>4} "
              f"({held})")
    if args.dump:
        check_dump(args.dump, matched[-1][1], matched)
    print(f"game: {len(matched)} checkpoints match the original")
    return 0


if __name__ == "__main__":
    sys.exit(main())
