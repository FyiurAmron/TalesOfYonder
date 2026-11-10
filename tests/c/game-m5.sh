#!/bin/sh
# M5 gameplay gate: run the transliterated game through the scripted slice --
# title, character creation (CONAN with the sling and dagger), keep/roster,
# game start, the walk to the garden, the spider fight, and the chest loot --
# and match the original's frames and the settled party state.
#
#   sh tests/c/game-m5.sh                 # build if needed
#   sh tests/c/game-m5.sh --no-build      # use build/c/yt2game as it is
#   sh tests/c/game-m5.sh --ticks 9000    # run longer
#
# The script is tests/c/m5_input.txt; the expected frames are in
# tests/c/m5_checkpoints.txt, taken from the original's fixed-seam recording
# (build/tmp/or3) driven through the same key and mouse sequence.  Artifacts
# land in build/runs/m5/<stamp>/; exits non-zero on any mismatch.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/m5/$STAMP"
GAME="$BUILD/yt2game"
TICKS=9000
TRANSFERS=150000000
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        --transfers) TRANSFERS=${2:?--transfers needs a value}; shift 2 ;;
        *)
            echo "m5: unknown option $1" >&2
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
        echo "m5: build failed" >&2
        exit 1
    fi
fi

mkdir -p "$RUN/game" "$RUN/checkpoint"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$file" "$RUN/game/$file"
done

set +e
# The resident SBFMDRV changes intro and script timing; this recording
# pins the game's silent path so the scripted ticks stay aligned.
timeout 300s "$GAME" --game "$RUN/game" --link --ticks "$TICKS" --no-sound \
    --transfers "$TRANSFERS" --pin-dos-time 0x1234 \
    --input-script "$ROOT/tests/c/m5_input.txt" \
    --present-log "$RUN/checkpoint/present.txt" \
    --dump "$RUN/checkpoint" >"$RUN/checkpoint/stdout.log" \
    2>"$RUN/checkpoint/stderr.log"
STATUS=$?
set -e
if [ "$STATUS" -ne 0 ]; then
    cat "$RUN/checkpoint/stderr.log" >&2
    echo "m5: runner failed with status $STATUS" >&2
    exit 1
fi
if ! grep -q 'exited=0' "$RUN/checkpoint/stderr.log"; then
    cat "$RUN/checkpoint/stderr.log" >&2
    echo "m5: the game exited early" >&2
    exit 1
fi

python3 "$ROOT/tests/c/game_frames.py" "$RUN/checkpoint/present.txt" \
    "$ROOT/tests/c/m5_checkpoints.txt" --min-hold 1 --hold-to frame \
    --globals m5 --dump "$RUN/checkpoint"

echo "m5: OK ($RUN)"
