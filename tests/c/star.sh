#!/bin/sh
# Check the shooting star on the attract gate.  The star is drawn directly
# into the work buffer and flushed with one full-screen REP MOVSW per step.
# At the default 3,000 cycles/ms one 64 KB copy is ~13 ms, so the 52-step
# streak lasts ~0.7 s and the 60 Hz display samples it every one or two
# steps: a visible descent, the way the game looks on its minimum machine.
#
#   sh tests/c/star.sh              # build if needed, run 1200 ticks
#   sh tests/c/star.sh --no-build   # use build/c/yt2game as it is
#   sh tests/c/star.sh --build      # force a rebuild
#   sh tests/c/star.sh --ticks N    # run longer (the gate is ~tick 600)
#
# The star path is x + y = 276, x = 268, 266, ... 164: the loop starts at
# (268,8), moves two pixels left and two down, and its colour starts at 1Fh
# and falls to 0Bh on the last 20 steps.  The star is the only path pixel
# whose index matches that ramp; static 2x2 stars and the title star are off
# the line.  Blank presents (between the erase and the next draw) have no
# hit and are simply skipped.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/star/$STAMP"
GAME="$BUILD/yt2game"
TICKS=1200
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "star: unknown option $1" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN/frames"
if [ "$DO_BUILD" = yes ] || { [ "$DO_BUILD" = auto ] && [ ! -x "$GAME" ]; } ||
   [ -n "$(find "$ROOT/engine/c/src" "$ROOT/engine/c/include" \
                 "$ROOT/engine/c/Makefile" "$ROOT/engine/c/tools/build.sh" \
                 -newer "$GAME" -print -quit 2>/dev/null)" ]; then
    if ! sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1; then
        cat "$RUN/build.log" >&2
        echo "star: build failed" >&2
        exit 1
    fi
fi

mkdir -p "$RUN/game"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$file" "$RUN/game/$file"
done

timeout 120s "$GAME" --game "$RUN/game" --ticks "$TICKS" \
    --transfers 100000000 --present-dir "$RUN/frames" \
    --present-log "$RUN/present.txt" >"$RUN/stdout.log" 2>"$RUN/stderr.log"
if ! grep -q 'exited=0' "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "star: the game exited early" >&2
    exit 1
fi

python3 - "$RUN/frames" <<'PY'
import sys
from pathlib import Path

frames = Path(sys.argv[1])
STEPS = 53


def path(n):
    return 268 - 2 * n, 8 + 2 * n


def star_colour(n):
    return 0x1F if n <= 32 else 0x1F - (n - 32)


samples = []
for file in sorted(frames.glob("frame_*.idx")):
    frame = int(file.stem.split("_")[1])
    index = file.read_bytes()
    hits = [n for n in range(STEPS)
            if index[path(n)[1] * 320 + path(n)[0]] == star_colour(n)]
    if len(hits) == 1:
        samples.append((frame, hits[0]))

runs = []
current = []
for frame, n in samples:
    if current and n <= current[-1][1]:
        if len(current) > len(runs):
            runs.append(current)
        current = []
    current.append((frame, n))
if len(current) > len(runs):
    runs.append(current)

if not runs:
    raise SystemExit("star: no sampled frames on the gate path")
run = max(runs, key=len)
steps = [entry[1] for entry in run]
if len(run) < 20:
    raise SystemExit(
        f"star: only {len(run)} sampled position(s) ({steps}); want >= 20")
if steps[0] > 4:
    raise SystemExit(f"star: the streak starts at step {steps[0]}, not the path top")
if steps[-1] < STEPS - 3:
    raise SystemExit(
        f"star: the streak ends at step {steps[-1]}, not the path end")
gaps = [b - a for a, b in zip(steps, steps[1:])]
if any(gap < 1 or gap > 4 for gap in gaps):
    raise SystemExit(
        f"star: sampled positions are not the expected descent: {steps}")
print(f"star: OK ({len(run)} samples at frames "
      f"{run[0][0]}..{run[-1][0]}, steps {steps[0]}..{steps[-1]}, "
      f"gaps {min(gaps)}..{max(gaps)})")
PY

# Keep the log but not the frame dump.
rm -rf "$RUN/frames"
echo "star: artifacts in $RUN"
