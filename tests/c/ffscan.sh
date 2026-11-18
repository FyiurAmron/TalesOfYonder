#!/bin/sh
# Boot save-scan gate: stage saves in slots 1 and 3 (one named, one with an
# empty name field), boot to tick 400, and require the boot scan to mark
# exactly those slots found with the file headers as names.
#
#   sh tests/c/ffscan.sh              # build if needed
#   sh tests/c/ffscan.sh --no-build   # use build/c/yt2game as it is
#
# Artifacts land in build/runs/ffscan/<stamp>/.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/ffscan/$STAMP"
GAME="$BUILD/yt2game"
TICKS=400
TRANSFERS=50000000
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        *) echo "ffscan: unknown option $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$RUN/game" "$RUN/dump"
if [ "$DO_BUILD" = yes ] || [ ! -x "$GAME" ] || \
   [ -n "$(find "$ROOT/engine/c/src" "$ROOT/engine/c/include" \
                 "$ROOT/engine/asm/swreg/src" "$ROOT/tools/port" \
                 "$ROOT/engine/c/Makefile" "$ROOT/engine/c/tools/build.sh" \
                 -newer "$GAME" -print -quit 2>/dev/null)" ]; then
    if ! sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1; then
        cat "$RUN/build.log" >&2
        echo "ffscan: build failed" >&2
        exit 1
    fi
fi

for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/YT2/$file" "$RUN/game/$file"
done
cp "$ROOT/tests/data/m5-room-curgame" "$RUN/game/SAVGAME1"
cp "$ROOT/tests/port-combat/spider.sav" "$RUN/game/SAVGAME3"

set +e
timeout 120s "$GAME" --game "$RUN/game" --ticks "$TICKS" --no-sound \
    --transfers "$TRANSFERS" --pin-dos-time 0x1234 --dump "$RUN/dump" \
    >"$RUN/stdout.log" 2>"$RUN/stderr.log"
STATUS=$?
set -e
if [ "$STATUS" -ne 0 ]; then
    cat "$RUN/stderr.log" >&2
    echo "ffscan: runner failed with status $STATUS" >&2
    exit 1
fi
if ! grep -q 'exited=0' "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "ffscan: the game exited early" >&2
    exit 1
fi

python3 "$ROOT/tests/c/ffscan.py" "$RUN/dump" "$RUN/game"
echo "ffscan: OK (artifacts in $RUN)"
