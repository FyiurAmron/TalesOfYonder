#!/bin/sh
# Check the live folder link end to end: the C engine's intro must reach the
# client without the client collapsing a burst of frame_ends into one picture.
#
# The title star and the intro's DAC-paced fades produce several complete
# frames inside one BIOS tick.  The engine writes those records back to back,
# so the client needs a frame queue: with only one pending flag, `shown` stays
# well below `frames` and the single-tick steps are never displayed.
#
#   sh tests/c/live.sh              # build if needed, run 320 ticks (~18 s)
#   sh tests/c/live.sh --no-build   # use build/c/yt2game and the client as they are
#   sh tests/c/live.sh --ticks 400  # run longer
#
# Artifacts land in build/runs/live/<stamp>/.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
CLIENT="$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/live/$STAMP"
GAME="$BUILD/yt2game"
TICKS=320
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "live: unknown option $1" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN"
if [ "$DO_BUILD" = yes ] || { [ "$DO_BUILD" = auto ] && [ ! -x "$GAME" ]; }; then
    sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1 || {
        cat "$RUN/build.log" >&2
        exit 1
    }
fi
if [ "$DO_BUILD" = yes ] || [ "$DO_BUILD" = auto ] || [ ! -f "$CLIENT" ]; then
    dotnet build "$ROOT/client" -c Release >"$RUN/client-build.log" 2>&1 || {
        cat "$RUN/client-build.log" >&2
        exit 1
    }
fi

mkdir -p "$RUN/game"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$file" "$RUN/game/$file"
done

"$GAME" --game "$RUN/game" --live --ticks "$TICKS" \
    --present-log "$RUN/present.txt" >"$RUN/engine.log" 2>&1 &
ENGINE=$!
trap 'kill "$ENGINE" 2>/dev/null || true' EXIT INT TERM

sleep 0.3
WAIT=$((TICKS / 18 + 8))
timeout "${WAIT}s" dotnet "$CLIENT" \
    --follow "$RUN/game/OUTPUT.BIN" --input "$RUN/game/INPUT.BIN" \
    --headless --no-audio --stats 1 --connect-timeout 5 \
    >"$RUN/client.log" 2>&1 || true
wait "$ENGINE" 2>/dev/null || true

LAST=$(grep 'frames=' "$RUN/client.log" | tail -1 || true)
if [ -z "$LAST" ]; then
    echo "live: the client printed no stats" >&2
    cat "$RUN/client.log" >&2
    exit 1
fi
FRAMES=$(printf '%s\n' "$LAST" | sed -n 's/.*frames=\([0-9][0-9]*\).*/\1/p')
SHOWN=$(printf '%s\n' "$LAST" | sed -n 's/.*shown=\([0-9][0-9]*\).*/\1/p')
DROPPED=$(printf '%s\n' "$LAST" | sed -n 's/.*dropped=\([0-9][0-9]*\).*/\1/p')
DROPPED=${DROPPED:-0}

if [ -z "$FRAMES" ] || [ -z "$SHOWN" ]; then
    echo "live: cannot read frames/shown from: $LAST" >&2
    exit 1
fi
if [ "$FRAMES" -lt 100 ]; then
    echo "live: only $FRAMES frames reached the client (want >= 100)" >&2
    exit 1
fi
if [ "$DROPPED" -ne 0 ]; then
    echo "live: the client dropped $DROPPED frames: $LAST" >&2
    exit 1
fi
if [ "$SHOWN" -lt "$FRAMES" ]; then
    echo "live: client decoded $FRAMES frames but showed only $SHOWN" >&2
    exit 1
fi
echo "live: OK (frames=$FRAMES, shown=$SHOWN, dropped=$DROPPED)"
echo "live: artifacts in $RUN"
