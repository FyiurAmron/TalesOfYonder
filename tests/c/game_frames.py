#!/usr/bin/env python3
"""Check the transliterated game's presented frames against checkpoints.

    python3 tests/c/game_frames.py PRESENT.txt tests/c/game_checkpoints.txt
    python3 tests/c/game_frames.py PRESENT.txt tests/c/game_checkpoints.txt \
        --dump build/runs/c/<stamp>/checkpoint

The present log comes from `yt2game --present-log`: one line per distinct
frame, `<frame> <tick> <fnv-fb> <fnv-palette>`.  The checkpoint file lists the
scenes the original displayed in that order with the same hashes; `*` in the
palette column matches any palette for an animated screen.

The checker requires every checkpoint pair to appear in order and to stay on
screen for `--min-hold` engine ticks before the next change (the last one may
run to the end of the log).  By default a hold is measured to the next present
record; `--hold-to frame` measures it to the next framebuffer change instead,
which is what the M5 gate uses because one screen emits several palette
records per tick.  A checkpoint file may also pin a hold with a fourth field;
the observed hold must then stay within `--hold-tolerance` ticks of it.  With
`--dump` it also verifies that the raw framebuffer/palette dump at the end of
the run hashes to the final checkpoint and, with `--globals`, that the settled
state matches (the M3 attract globals by default, the M5 party state with
`--globals m5`).
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

# Settled state at the end of the M5 slice: the original's save state at the
# chest screen (spider defeated, party in the garden) and the port's end dump
# after the chest closes agree.  The party record in slot 0 carries the typed
# name.
M5_GLOBALS = (("v_94E7", 0x94E7, 0x00A1),   # party x
              ("v_94E9", 0x94E9, 0x0022),   # party y
              ("v_94E5", 0x94E5, 0x4000),   # facing south
              ("v_5062", 0x5062, 0x0644),   # combat/chest state
              ("v_5064", 0x5064, 0x0001))   # world state
M5_STRINGS = (("slot0-name", 0x9643, b"CONAN"),)


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
        if len(parts) not in (3, 4):
            raise SystemExit(f"game: bad checkpoint line {number}: {line!r}")
        hold = None
        if len(parts) == 4 and parts[3] != "-":
            try:
                hold = int(parts[3])
            except ValueError:
                raise SystemExit(
                    f"game: bad checkpoint hold line {number}: {line!r}")
        palette = parts[2].lower()
        if palette in ("*", "-"):
            palette = None
        items.append((parts[0], parts[1].lower(), palette, hold))
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


def check_sequence(rows, checkpoints, min_hold, tolerance, hold_to="record"):
    cursor = 0
    matched = []
    for name, fb, pal, expected_hold in checkpoints:
        found = None
        for index in range(cursor, len(rows)):
            if rows[index][2] == fb and (pal is None or rows[index][3] == pal):
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
        if hold_to == "frame":
            for index in range(found + 1, len(rows)):
                if rows[index][2] != rows[found][2]:
                    hold = rows[index][1] - rows[found][1]
                    break
        elif found + 1 < len(rows):
            hold = rows[found + 1][1] - rows[found][1]
        if hold is not None:
            if hold < min_hold:
                raise SystemExit(
                    f"game: checkpoint {name} at frame {rows[found][0]} "
                    f"(tick {rows[found][1]}) lasts only {hold} ticks "
                    f"(need {min_hold})")
            if expected_hold is not None and abs(hold - expected_hold) > tolerance:
                raise SystemExit(
                    f"game: checkpoint {name} at frame {rows[found][0]} "
                    f"(tick {rows[found][1]}) holds {hold} ticks, expected "
                    f"{expected_hold} +/- {tolerance}")
        matched.append((name, rows[found], hold, expected_hold))
        cursor = found + 1
    return matched


def check_dump(directory, last_row, animated=False):
    with open(f"{directory}/video.bin", "rb") as stream:
        video = fnv1a64(stream.read())
    if f"{video:016x}" != last_row[2]:
        raise SystemExit(
            f"game: end dump framebuffer is {video:016x}, expected "
            f"{last_row[2]} (last presented frame)")
    if animated:
        # The palette keeps cycling after the last present on an animated
        # screen (the chest and fight screens); only the pixels are settled.
        return video, None
    with open(f"{directory}/palette.bin", "rb") as stream:
        palette = fnv1a64(stream.read())
    if f"{palette:016x}" != last_row[3]:
        raise SystemExit(
            f"game: end dump palette is {palette:016x}, expected "
            f"{last_row[3]} (last presented frame)")
    return video, palette


def check_globals(directory, table=GLOBALS, strings=()):
    with open(f"{directory}/dgroup.bin", "rb") as stream:
        dgroup = stream.read()
    for name, offset, expected in table:
        value = struct.unpack_from("<H", dgroup, offset)[0]
        if value != expected:
            raise SystemExit(f"game: {name} is {value:04X}, expected "
                             f"{expected:04X}")
    for name, offset, expected in strings:
        value = dgroup[offset:offset + len(expected)]
        if value != expected:
            raise SystemExit(f"game: {name} is {value!r}, expected {expected!r}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("checkpoints")
    ap.add_argument("--min-hold", type=int, default=60,
                    help="ticks a checkpoint must stay on screen (default 60)")
    ap.add_argument("--hold-tolerance", type=int, default=2,
                    help="ticks a pinned hold may differ (default 2)")
    ap.add_argument("--hold-to", choices=("record", "frame"), default="record",
                    help="measure a hold to the next present record (default) "
                         "or to the next framebuffer change")
    ap.add_argument("--dump", help="checkpoint dump directory from --dump")
    ap.add_argument("--globals", choices=("m3", "m5", "none"), default="m3",
                    help="settled state to check when the run ends on the "
                         "last checkpoint (default m3)")
    args = ap.parse_args()

    rows = read_log(args.log)
    checkpoints = read_checkpoints(args.checkpoints)
    matched = check_sequence(rows, checkpoints, args.min_hold,
                             args.hold_tolerance, args.hold_to)
    for name, row, hold, expected in matched:
        held = "to the end" if hold is None else f"{hold} ticks"
        if expected is not None and hold is not None:
            held += f", expected {expected}"
        print(f"game: {name:<14} frame {row[0]:>4} tick {row[1]:>4} "
              f"({held})")
    if args.dump:
        last = checkpoints[-1]
        check_dump(args.dump, rows[-1], animated=last[2] is None)
        if rows[-1][2] == last[1] and (last[2] is None or
                                       rows[-1][3] == last[2]):
            if args.globals == "m3":
                check_globals(args.dump)
            elif args.globals == "m5":
                check_globals(args.dump, M5_GLOBALS, M5_STRINGS)
            else:
                print("game: globals not checked")
        else:
            print("game: run ends past the last checkpoint; "
                  "globals not checked")
    print(f"game: {len(matched)} checkpoints match the original")
    return 0


if __name__ == "__main__":
    sys.exit(main())
