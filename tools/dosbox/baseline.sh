#!/bin/sh
# Headless no-input baseline of the ORIGINAL game under DOSBox-X.
#
#   sh tools/dosbox/baseline.sh [duration_seconds]     (default 75)
#
# Runs _ref/orig-bin on a private Xvfb display with the oracle profile
# (tools/dosbox/dosbox-x.conf): X window snapshots (snap.py) at fixed checkpoints,
# save states from the built-in autosave (slots 1..4), everything collected
# under build/runs/oracle/<timestamp>/.
#
# Requirements: Xvfb, dosbox-x, python3 (Pillow; libX11 at runtime).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
DURATION="${1:-75}"
STAMP="$(date +%Y%m%d-%H%M%S)"
RUN="$ROOT/build/runs/oracle/$STAMP"
REFBIN="$ROOT/_ref/orig-bin"
DISP=":97"
GAME_FILES="CH2.BAT SBFMDRV.COM SWREG.EXE WORLD.DAT PICTURES.VGA CURGAME"

if [ -e /tmp/.X11-unix/X97 ]; then
    echo "baseline: display :97 is busy; refusing (stale Xvfb?)" >&2
    exit 1
fi

mkdir -p "$RUN/capture" "$RUN/saves" "$RUN/game"
( cd "$REFBIN" && md5sum -c "$ROOT/engine/asm/swreg/tools/SWREG.EXE.md5" ) >"$RUN/oracle-md5.txt"
for f in $GAME_FILES; do
    cp "$REFBIN/$f" "$RUN/game/$f"
done

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >"$RUN/xvfb.log" 2>&1 &
XVFB=$!
trap 'kill $XVFB 2>/dev/null' EXIT

i=0
while [ ! -e /tmp/.X11-unix/X97 ] && [ "$i" -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done

cd "$RUN"
DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dosbox-x -conf "$HERE/dosbox-x.conf" -set "dosbox captures=$RUN/capture" \
    -fastlaunch -time-limit "$DURATION" \
    -savedir "$RUN/saves" >"$RUN/stdout.log" 2>&1 &
DB=$!

# screen snapshots at fixed checkpoints (XGetImage; DOSBox-X 2026.01.02's own
# F12+P screenshot aborts on the game's VESA mode, so we do not use it)
(
    sleep 15 && python3 "$HERE/snap.py" --display "$DISP" --window "$RUN/capture/screen_015.png" || true
    sleep 20 && python3 "$HERE/snap.py" --display "$DISP" --window "$RUN/capture/screen_035.png" || true
    sleep 20 && python3 "$HERE/snap.py" --display "$DISP" --window "$RUN/capture/screen_055.png" || true
) &
SNAP=$!

wait "$DB" || true
kill "$SNAP" 2>/dev/null || true

echo "run dir:  $RUN"
echo "oracle:   $(tail -1 "$RUN/oracle-md5.txt")"
echo "captures: $(ls "$RUN/capture" 2>/dev/null | wc -l)"
echo "saves:    $(ls "$RUN/saves" 2>/dev/null | wc -l)"
