#!/bin/sh
# Sound-effect gate: run the scripted M5 opening with the resident driver on
# and check that the emitted fn_PlaySFX hook reports the game's WORLD.DAT VOC
# effects (AUDIO sample) instead of only beeping for effect 3.
#
#   sh tests/c/sfx.sh              # build if needed
#   sh tests/c/sfx.sh --no-build   # use build/c/yt2game as it is
#
# Artifacts land in build/runs/sfx/<stamp>/; exits non-zero when the log has
# no sample requests or an offset is not a VOC record.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/sfx/$STAMP"
GAME="$BUILD/yt2game"
TICKS=3000
TRANSFERS=150000000
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "sfx: unknown option $1" >&2
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
        echo "sfx: build failed" >&2
        exit 1
    fi
fi

mkdir -p "$RUN/game"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/YT2/$file" "$RUN/game/$file"
done

set +e
# The scripted opening reaches the name/roster SFX; sound stays on so the
# resident driver and the fn_PlaySFX hook take the real path.  The fixed DOS
# time keeps the RNG-driven timing identical between runs.
timeout 300s "$GAME" --game "$RUN/game" --link --ticks "$TICKS" --sound \
    --transfers "$TRANSFERS" --pin-dos-time 0x1234 \
    --input-script "$ROOT/tests/c/m5_input.txt" \
    --audio-log "$RUN/audio.log" \
    --present-log "$RUN/present.txt" >"$RUN/stdout.log" \
    2>"$RUN/stderr.log"
STATUS=$?
set -e
if [ "$STATUS" -ne 0 ]; then
    cat "$RUN/stderr.log" >&2
    echo "sfx: runner failed with status $STATUS" >&2
    exit 1
fi
if ! grep -q 'exited=0' "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "sfx: the game exited early" >&2
    exit 1
fi

python3 "$ROOT/tests/c/sfx.py" "$RUN/audio.log" "$RUN/game/WORLD.DAT"
echo "sfx: OK (artifacts in $RUN)"
