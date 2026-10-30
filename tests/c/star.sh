#!/bin/sh
# Check the shooting star on the attract gate.  The star is drawn directly
# into the work buffer and flushed with one full-screen REP MOVSW per step;
# the whole streak lasts less than one BIOS tick.  Without a present at that
# flush it falls between passes and never reaches the client.
#
#   sh tests/c/star.sh              # build if needed, run 400 ticks
#   sh tests/c/star.sh --no-build   # use build/c/yt2game as it is
#   sh tests/c/star.sh --build      # force a rebuild
#
# The star path is x + y = 276, x = 268, 266, ... 206 (the loop starts at
# (268,8) and moves two pixels left and two down).  A frame must show exactly
# one bright pixel on that path; fixed 2x2 stars and the title star are off it.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/star/$STAMP"
GAME="$BUILD/yt2game"
TICKS=400
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
hits = []
for path in sorted(frames.glob("frame_*.idx")):
    frame = int(path.stem.split("_")[1])
    idx = path.read_bytes()
    pal = (frames / f"frame_{frame:05d}.pal").read_bytes()
    bright = [entry for entry in range(256)
              if min(pal[entry * 3:entry * 3 + 3]) >= 50]
    found = []
    for y in range(8, 71):
        x = 276 - y
        if 206 <= x <= 268 and idx[y * 320 + x] in bright:
            found.append((x, y))
    if len(found) == 1:
        hits.append((frame, found[0]))

positions = [p for _, p in hits]
distinct = []
for position in positions:
    if position not in distinct:
        distinct.append(position)
if len(hits) < 2 or len(distinct) < 2:
    raise SystemExit(
        f"star: only {len(hits)} frame(s) with the gate star "
        f"({len(distinct)} position(s)); want at least 2")
steps = [b[0] - a[0] for a, b in zip(distinct, distinct[1:])]
if any(step != -2 for step in steps):
    raise SystemExit(f"star: the gate star did not move by two: {distinct}")
print(f"star: OK ({len(hits)} frames at frames "
      f"{hits[0][0]}..{hits[-1][0]}, positions {distinct})")
PY

# Keep the log but not the frame dump.
rm -rf "$RUN/frames"
echo "star: artifacts in $RUN"
