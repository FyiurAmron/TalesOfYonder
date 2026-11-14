#!/bin/sh
# Canned-input gate for the B2 player: drive yt2p-play with a hand-written
# INPUT.BIN (no GUI, no client) and require the game-key flow: arrows move
# the catalog selection, Enter opens the selected map, ESC returns, a click
# selects a row, a marker click shows the legend, MORE clicks page the
# window, a double-click opens, F2 opens the monster list with its own
# selection, Enter a monster's statistics, F3 the spell list with its own,
# selection, Enter a spell's detail page,
# F4 the class list, Enter a line's spells, F5 the categories, Enter a
# category's items, Enter an item's page, F6 the walkthrough, PgDn/PgUp turn
# its pages, and quit
# exits cleanly.
#
#   sh tests/port-input/run.sh
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
RUN="$ROOT/build/runs/port-input/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$RUN/game"

make -C "$ROOT/engine/port" BUILD="$ROOT/build/port" all >"$RUN/build.log" 2>&1
cp "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" "$ROOT/_ref/orig-bin/YT2/PICTURES.VGA" \
    "$RUN/game/"

"$ROOT/build/port/yt2p-play" "$RUN/game" --tick-ms 10 >"$RUN/play.log" 2>&1 &
PPID=$!
trap 'kill "$PPID" 2>/dev/null || true; wait 2>/dev/null || true' EXIT INT TERM
sleep 1
# One step per round trip: hello, keyframe, Down, Down, Enter, ESC, legend
# pick (Up, Up, Enter, marker click, ESC), two MORE-downs, one MORE-up,
# four Ups and a double-click (opens plus the stale-latch legend), ESC,
# F1, a click on catalog row 5 (abs + button together), F2, Down, a
# double-click on monster row 0, ESC, Down, F3, Down, ESC, F4, Down, Enter,
# Down, a click on the detail footer (back to the list), ESC, F5, Down,
# Enter (attribute info), ESC, six Downs (weapons), Enter, Down, ESC, ESC,
# F6, PgDn, PgUp, MORE-down, MORE-up, ESC, then quit.  Keys drain one
# per tick, so each step presents before the next lands; steps stay 30
# ticks apart so only the double-click batches (two presses, one tick)
# open.
python3 - "$RUN/game/INPUT.BIN" <<'EOF'
import struct
import sys
import time

path = sys.argv[1]
seq = [0]


def frame(family, payload):
    header = struct.pack("<HBBBBII", 0x5954, 0, family, 0, 0, seq[0],
                         len(payload))
    seq[0] += 1
    return header + payload


def record(payload):
    return struct.pack("<II", 1000 + seq[0], len(payload)) + payload


def emit(stream, *frames):
    for index, item in enumerate(frames):
        stream.write(record(frame(item[0], item[1])))
    stream.flush()


with open(path, "wb") as stream:
    stream.write(struct.pack("<4sHHIB3x", b"YT2B", 1, 16, 1193182, 1))
    stream.flush()
    emit(stream, (5, bytes([0, 1, 0])), (5, bytes([4])))
    steps = [
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> sel 1
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> sel 2
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> overhead
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> catalog
        [(3, bytes([0, 1, 0x48, 0x00]))],  # Up -> sel 1
        [(3, bytes([0, 1, 0x48, 0x00]))],  # Up -> sel 0
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> map 67
        [(3, bytes([3]) + struct.pack("<HH", 172, 60)),
         (3, bytes([2, 0, 1]))],           # click marker -> legend 116
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> catalog
        [(3, bytes([3]) + struct.pack("<HH", 186, 170)),
         (3, bytes([2, 0, 1]))],           # MORE-down -> sel 14
        [(3, bytes([3]) + struct.pack("<HH", 186, 170)),
         (3, bytes([2, 0, 1]))],           # MORE-down -> sel 28
        [(3, bytes([3]) + struct.pack("<HH", 132, 170)),
         (3, bytes([2, 0, 1]))],           # MORE-up -> sel 14
        [(3, bytes([0, 1, 0x48, 0x00]))],  # Up -> sel 13
        [(3, bytes([0, 1, 0x48, 0x00]))],  # Up -> sel 12
        [(3, bytes([0, 1, 0x48, 0x00]))],  # Up -> sel 11
        [(3, bytes([0, 1, 0x48, 0x00]))],  # Up -> sel 10
        [(3, bytes([3]) + struct.pack("<HH", 184, 128)),
         (3, bytes([2, 0, 1])),
         (3, bytes([2, 0, 1]))],           # double-click row 10 -> map 91
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> catalog
        [(3, bytes([0, 1, 0x3B, 0x00]))],  # F1 -> catalog
        [(3, bytes([3]) + struct.pack("<HH", 100, 80)),
         (3, bytes([2, 0, 1]))],           # click row 5 -> sel 5
        [(3, bytes([0, 1, 0x3C, 0x00]))],  # F2 -> monsters
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> monster sel 1
        [(3, bytes([3]) + struct.pack("<HH", 100, 32)),
         (3, bytes([2, 0, 1])),
         (3, bytes([2, 0, 1]))],           # double-click row 0 -> detail
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> monsters
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> monster sel 1
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> monster detail
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> monsters
        [(3, bytes([0, 1, 0x3D, 0x00]))],  # F3 -> spells
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> spell sel 1
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> spell detail
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> spells
        [(3, bytes([3]) + struct.pack("<HH", 100, 32)),
         (3, bytes([2, 0, 1])),
         (3, bytes([2, 0, 1]))],           # double-click row 0 -> detail
        [(3, bytes([3]) + struct.pack("<HH", 29, 188)),
         (3, bytes([2, 0, 1]))],           # click footer -> spells
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> help
        [(3, bytes([0, 1, 0x3E, 0x00]))],  # F4 -> classes
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> class sel 1
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> alchemist spells
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> detail sel 1
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> spell detail
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> alchemist spells
        [(3, bytes([3]) + struct.pack("<HH", 29, 188)),
         (3, bytes([2, 0, 1]))],           # click footer -> classes
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> help
        [(3, bytes([0, 1, 0x3F, 0x00]))],  # F5 -> categories
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> attribute
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> attribute info
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> categories
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> jewels
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> scrolls
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> potions
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> supplies
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> transport
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> weapons
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> weapon list
        [(3, bytes([0, 1, 0x50, 0x00]))],  # Down -> weapon sel 1
        [(3, bytes([0, 1, 0x1C, 0x0D]))],  # Enter -> item page
        [(3, bytes([3]) + struct.pack("<HH", 55, 145)),
         (3, bytes([2, 0, 1]))],           # click +1 box
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> weapon list
        [(3, bytes([3]) + struct.pack("<HH", 100, 37)),
         (3, bytes([2, 0, 1])),
         (3, bytes([2, 0, 1]))],           # double-click row 1 -> page
        [(3, bytes([3]) + struct.pack("<HH", 29, 188)),
         (3, bytes([2, 0, 1]))],           # click footer -> weapon list
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> categories
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> help
        [(3, bytes([0, 1, 0x40, 0x00]))],  # F6 -> walkthrough
        [(3, bytes([0, 1, 0x51, 0x00]))],  # PgDn -> page 1
        [(3, bytes([0, 1, 0x49, 0x00]))],  # PgUp -> page 0
        [(3, bytes([3]) + struct.pack("<HH", 186, 170)),
         (3, bytes([2, 0, 1]))],           # MORE-down -> page 1
        [(3, bytes([3]) + struct.pack("<HH", 132, 170)),
         (3, bytes([2, 0, 1]))],           # MORE-up -> page 0
        [(3, bytes([0, 1, 0x01, 0x1B]))],  # ESC -> help
    ]
    for batch in steps:
        time.sleep(0.3)
        emit(stream, *batch)
    time.sleep(0.3)
    emit(stream, (3, bytes([3])))          # quit
print("input: canned script done")
EOF
sleep 1
if kill -0 "$PPID" 2>/dev/null; then
    echo "port-input: FAIL - the player ignored quit" >&2
    cat "$RUN/play.log" >&2
    exit 1
fi
wait "$PPID" 2>/dev/null || true
grep -q "scene 1 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - first Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 1 sel 2" "$RUN/play.log" ||
    { echo "port-input: FAIL - second Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 2 sel 2" "$RUN/play.log" ||
    { echo "port-input: FAIL - enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 2 sel 0 leg -1" "$RUN/play.log" ||
    { echo "port-input: FAIL - map 67 missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 2 sel 0 leg 116" "$RUN/play.log" ||
    { echo "port-input: FAIL - legend missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 1 sel 28$" "$RUN/play.log" ||
    { echo "port-input: FAIL - MORE-down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 1 sel 14$" "$RUN/play.log")" -ge 2 ||
    { echo "port-input: FAIL - MORE-up missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 2 sel 10 leg -1" "$RUN/play.log" ||
    { echo "port-input: FAIL - double-click missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 2 sel 10 leg 98" "$RUN/play.log" ||
    { echo "port-input: FAIL - stale latch missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 1 sel 5" "$RUN/play.log" ||
    { echo "port-input: FAIL - click missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 5 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - F2 missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 5 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - monster Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 12 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - monster double-click missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 12 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - monster Enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 5 sel 1" "$RUN/play.log")" -ge 2 ||
    { echo "port-input: FAIL - detail ESC missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 6 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - F3 missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 6 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - spell Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 13 sel 43" "$RUN/play.log" ||
    { echo "port-input: FAIL - spell Enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 13 sel 48" "$RUN/play.log" ||
    { echo "port-input: FAIL - spell double-click missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 6 sel 0$" "$RUN/play.log")" -ge 2 ||
    { echo "port-input: FAIL - spell footer missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 7 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - F4 missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 7 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - class Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 8 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - class Enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 8 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - detail Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 13 sel 67" "$RUN/play.log" ||
    { echo "port-input: FAIL - class spell Enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 7 sel 1" "$RUN/play.log")" -ge 2 ||
    { echo "port-input: FAIL - footer click missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 9 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - F5 missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 10 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - category Enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 9 sel 7" "$RUN/play.log" ||
    { echo "port-input: FAIL - category Downs missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 10 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - weapon Down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 14 sel 482" "$RUN/play.log" ||
    { echo "port-input: FAIL - item Enter missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 14 sel 482$" "$RUN/play.log")" -ge 3 ||
    { echo "port-input: FAIL - item +N/double-click missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 11 sel 0" "$RUN/play.log" ||
    { echo "port-input: FAIL - F6 missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 11 sel 1" "$RUN/play.log" ||
    { echo "port-input: FAIL - PgDn missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 11 sel 1$" "$RUN/play.log")" -ge 2 ||
    { echo "port-input: FAIL - walk MORE-down missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
test "$(grep -c "scene 11 sel 0$" "$RUN/play.log")" -ge 3 ||
    { echo "port-input: FAIL - walk MORE-up missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
grep -q "scene 0 sel 5" "$RUN/play.log" ||
    { echo "port-input: FAIL - walkthrough ESC missed" >&2; cat "$RUN/play.log" >&2; exit 1; }
echo "port-input: OK (artifacts in $RUN)"
