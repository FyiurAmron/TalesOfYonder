#!/bin/sh
# Run the transliterated game to the title/attract and check the M3
# checkpoints. Builds first when the runner is missing or stale.
#
#   sh tests/c/game.sh                 # build if needed, run 2500 ticks
#   sh tests/c/game.sh --no-build      # use build/c/yt2game as it is
#   sh tests/c/game.sh --build         # force a rebuild
#   sh tests/c/game.sh --ticks 3200    # run longer (needs more transfers)
#   sh tests/c/game.sh --ppm           # also write last.ppm/last.png
#
# Artifacts land in build/runs/game/<stamp>/; exits non-zero on any mismatch.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/game/$STAMP"
GAME="$BUILD/yt2game"
TICKS=2500
TRANSFERS=100000000
DO_BUILD=auto
WRITE_PPM=0

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        --transfers) TRANSFERS=${2:?--transfers needs a value}; shift 2 ;;
        --ppm) WRITE_PPM=1; shift ;;
        *)
            echo "game: unknown option $1" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN"
if [ "$DO_BUILD" = yes ] || [ ! -x "$GAME" ] ||
   [ -n "$(find "$ROOT/engine/c/src" "$ROOT/engine/c/include" \
                 "$ROOT/engine/asm/swreg/src" "$ROOT/tools/port" \
                 "$ROOT/engine/c/Makefile" "$ROOT/engine/c/tools/build.sh" \
                 -newer "$GAME" -print -quit 2>/dev/null)" ]; then
    if ! sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1; then
        cat "$RUN/build.log" >&2
        echo "game: build failed" >&2
        exit 1
    fi
fi

mkdir -p "$RUN/game" "$RUN/checkpoint"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$file" "$RUN/game/$file"
done

if [ "$WRITE_PPM" = 1 ]; then
    set -- --ppm "$RUN/last.ppm"
else
    set --
fi
set +e
timeout 300s "$GAME" --game "$RUN/game" --ticks "$TICKS" \
    --transfers "$TRANSFERS" --present-log "$RUN/checkpoint/present.txt" \
    --dump "$RUN/checkpoint" "$@" \
    >"$RUN/checkpoint/stdout.log" 2>"$RUN/checkpoint/stderr.log"
STATUS=$?
set -e
if [ "$STATUS" -ne 0 ]; then
    cat "$RUN/checkpoint/stderr.log" >&2
    echo "game: runner failed with status $STATUS" >&2
    exit 1
fi
if ! grep -q 'exited=0' "$RUN/checkpoint/stderr.log"; then
    cat "$RUN/checkpoint/stderr.log" >&2
    echo "game: the game exited early" >&2
    exit 1
fi

python3 "$ROOT/tests/c/game_frames.py" "$RUN/checkpoint/present.txt" \
    "$ROOT/tests/c/game_checkpoints.txt" --dump "$RUN/checkpoint"

if [ "$WRITE_PPM" = 1 ] && python3 -c 'import PIL' 2>/dev/null; then
    python3 - "$RUN/last.ppm" "$RUN/last.png" <<'PY'
import sys
from PIL import Image
Image.open(sys.argv[1]).save(sys.argv[2])
PY
fi

echo "game: OK ($RUN)"
