#!/bin/sh
# Build and run the STREAM build headless, then replay the recording.
#
#   sh tools/dosbox/stream.sh [duration_seconds]     (default 25)
#
# Unlike baseline.sh this runs the instrumented build from build/swreg/file/,
# records VIDEO messages to STREAM.BIN and renders them to PNG frames under
# build/runs/stream/<timestamp>/frames/.
#
# STREAM_KEYS optionally injects XTEST keys after start-up, as a
# space-separated list of SECONDS:CHORD items, e.g.
#
#   STREAM_KEYS="60:Return 65:space 70:Down" sh tools/dosbox/stream.sh 90
#
# The schedule is relative to the DOSBox-X launch, so the intro can be skipped
# to reach a menu or a game scene and trigger sound effects.  Set
# STREAM_SKIP_BUILD=1 to reuse build/swreg/file/ instead of assembling it.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
DURATION="${1:-25}"
STAMP="$(date +%Y%m%d-%H%M%S)"
RUN="$ROOT/build/runs/stream/$STAMP"
DISP=":97"

if [ -e /tmp/.X11-unix/X97 ]; then
    echo "stream: display :97 is busy; refusing (stale Xvfb?)" >&2
    exit 1
fi

if [ "${STREAM_SKIP_BUILD:-0}" != 1 ]; then
    sh "$ROOT/engine/asm/swreg/tools/build.sh" --variant file
fi

mkdir -p "$RUN/game" "$RUN/capture"
for f in SWREG.EXE CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp "$ROOT/build/swreg/file/$f" "$RUN/game/"
done

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >"$RUN/xvfb.log" 2>&1 &
XVFB=$!
KEYPID=
trap 'kill $XVFB 2>/dev/null || true; if [ -n "$KEYPID" ]; then kill "$KEYPID" 2>/dev/null || true; fi' EXIT

i=0
while [ ! -e /tmp/.X11-unix/X97 ] && [ "$i" -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done

cd "$RUN"
DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dosbox-x -conf "$HERE/dosbox-x.conf" -set "dosbox captures=$RUN/capture" \
    -fastlaunch -time-limit "$DURATION" >"$RUN/stdout.log" 2>&1 &
DB=$!

# Optional scripted input (STREAM_KEYS): sleep until each item's second mark,
# then send the chord to the DOSBox-X window through XTEST.
if [ -n "$STREAM_KEYS" ]; then
    (
        start=$(date +%s)
        for spec in $STREAM_KEYS; do
            at=${spec%%:*}
            chord=${spec#*:}
            while [ $(( $(date +%s) - start )) -lt "$at" ]; do
                sleep 0.2
            done
            python3 "$HERE/keys.py" --display "$DISP" "$chord" || true
        done
    ) &
    KEYPID=$!
fi

# one snapshot for a side-by-side check of the emulator screen
(
    sleep 15 && python3 "$HERE/snap.py" --display "$DISP" --window "$RUN/capture/screen_015.png" || true
) &
SNAP=$!

wait "$DB" || true
if [ -n "$KEYPID" ]; then
    kill "$KEYPID" 2>/dev/null || true
fi
kill "$SNAP" 2>/dev/null || true

if [ -f "$RUN/game/STREAM.BIN" ]; then
    mv "$RUN/game/STREAM.BIN" "$RUN/STREAM.BIN"
fi
if [ ! -f "$RUN/STREAM.BIN" ]; then
    echo "stream: no recording produced (allocation or create failed?)" >&2
    echo "run dir: $RUN"
    exit 1
fi

python3 "$ROOT/tools/stream/replay.py" "$RUN/STREAM.BIN" --out "$RUN/frames"

echo "run dir:  $RUN"
echo "recording: $(wc -c <"$RUN/STREAM.BIN") bytes"
echo "frames:   $(ls "$RUN/frames" 2>/dev/null | wc -l)"
