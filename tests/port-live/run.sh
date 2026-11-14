#!/bin/sh
# Live B2 proof: yt2p-play feeds the C# client over the folder link on a
# private Xvfb display, and scripted key/mouse input drives the engine back.
#
#   sh tests/port-live/run.sh
#
# Passes when the client window maps at 960x600, presents a 320x200 frame,
# the scripted keys boot through the intro (ESC, ESC, C) into the scene
# demo, and the engine answers the last key with a scene switch.  Needs
# Xvfb, xwininfo and dotnet; skips cleanly without them.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK="$ROOT/build/runs/port-live/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$WORK/game"

for tool in Xvfb xwininfo dotnet; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "port-live: SKIP - $tool is not installed"
        exit 0
    fi
done

make -C "$ROOT/engine/port" BUILD="$ROOT/build/port" all >"$WORK/build.log" 2>&1
dotnet build "$ROOT/client" -c Release >>"$WORK/build.log" 2>&1
cp "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" "$ROOT/_ref/orig-bin/YT2/PICTURES.VGA" \
    "$WORK/game/"

N=90
while [ "$N" -lt 120 ] && timeout 2 xwininfo -root -display ":$N" >/dev/null 2>&1; do
    N=$((N + 1))
done
if [ "$N" -ge 120 ]; then
    echo "port-live: FAIL - no free X display in :90..:119" >&2
    exit 1
fi
DISP=":$N"

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >"$WORK/xvfb.log" 2>&1 &
XPID=$!
PPID=
CPID=
cleanup() {
    [ -n "$CPID" ] && kill "$CPID" 2>/dev/null || true
    [ -n "$PPID" ] && kill "$PPID" 2>/dev/null || true
    kill "$XPID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM
I=0
until timeout 2 xwininfo -root -display "$DISP" >/dev/null 2>&1; do
    I=$((I + 1))
    if [ "$I" -ge 100 ]; then
        echo "port-live: FAIL - Xvfb did not come up on $DISP" >&2
        exit 1
    fi
    sleep 0.1
done

"$ROOT/build/port/yt2p-play" "$WORK/game" --tick-ms 55 >"$WORK/play.log" 2>&1 &
PPID=$!
# No sleep: the follower waits for OUTPUT.BIN itself, and every second
# counts against the VBE logo hold.
DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" \
    --follow "$WORK/game/OUTPUT.BIN" --input "$WORK/game/INPUT.BIN" \
    --stats 0 --key 2:0x01:0x1B --key 4:0x01:0x1B --key 6:0x2E:0x43 \
    --key 8:0x04:0x33 --mouse 10:20:10 \
    >"$WORK/client.log" 2>&1 &
CPID=$!

ID=
I=0
while [ -z "$ID" ] && [ "$I" -lt 100 ]; do
    ID=$(DISPLAY=$DISP xwininfo -root -tree 2>/dev/null |
         grep '"YT2 client"' | awk '{print $1}' | head -1)
    [ -n "$ID" ] || sleep 0.1
    I=$((I + 1))
done
if [ -z "$ID" ]; then
    echo "port-live: FAIL - no client window" >&2
    cat "$WORK/client.log" >&2
    exit 1
fi
INFO=$(DISPLAY=$DISP xwininfo -id "$ID" 2>&1)
if ! printf '%s\n' "$INFO" | grep -q "Map State: IsViewable"; then
    echo "port-live: FAIL - window hidden" >&2
    exit 1
fi
W=$(printf '%s\n' "$INFO" | awk '/Width:/{print $2}')
H=$(printf '%s\n' "$INFO" | awk '/Height:/{print $2}')
if [ "$W" != "960" ] || [ "$H" != "600" ]; then
    echo "port-live: FAIL - window ${W}x${H}, want 960x600" >&2
    exit 1
fi
I=0
until grep -q "client: first frame shown 640x480" "$WORK/client.log"; do
    I=$((I + 1))
    if [ "$I" -ge 150 ]; then
        echo "port-live: FAIL - no first frame" >&2
        cat "$WORK/client.log" >&2
        exit 1
    fi
    sleep 0.1
done
sleep 14
python3 "$ROOT/tools/dosbox/snap.py" --display "$DISP" --window \
    "$WORK/screen.png" >/dev/null
if ! grep -q "play: intro transition create" "$WORK/play.log"; then
    echo "port-live: FAIL - boot menu missed" >&2
    cat "$WORK/play.log" >&2
    exit 1
fi
if ! grep -q "present .* scene 2" "$WORK/play.log"; then
    echo "port-live: FAIL - engine ignored the key" >&2
    cat "$WORK/play.log" >&2
    exit 1
fi
echo "port-live: OK (artifacts in $WORK)"
