#!/bin/sh
# Check that the client really *shows* its window, not just creates one.
#
#   sh tests/window/run.sh               (needs Xvfb and xwininfo)
#
# SDL_CreateWindow succeeding proves nothing: a window created with
# SDL_WINDOW_HIDDEN is "created" too, the renderer still works, and every
# pixel gate keeps passing because they render offscreen - the client just
# looks console-only.  (That is not hypothetical: the flags constant once said
# 0x8, which is SDL_WINDOW_HIDDEN, not SDL_WINDOW_RESIZABLE.)  This gate runs
# the client on its own X server, replays the sample recording with --hold,
# and requires a mapped window of the expected geometry that has presented at
# least one frame.  It does that for the nearest neighbour presentation and
# for the xBRZ filter, which the client scales itself.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$ROOT/build/runs/window/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

for tool in Xvfb xwininfo; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "window: SKIP - $tool is not installed" >&2
        exit 0
    fi
done

# First display number nobody answers on (tests/link defaults to :88).  Ask
# the server rather than the socket directory: a /tmp/.X11-unix without the
# sticky bit makes Xvfb fall back to abstract sockets and leaves no file.
N=90
while [ "$N" -lt 120 ] && timeout 2 xwininfo -root -display ":$N" >/dev/null 2>&1; do
    N=$((N + 1))
done
if [ "$N" -ge 120 ]; then
    echo "window: FAIL - no free X display in :90..:119" >&2
    exit 1
fi
DISP=":$N"

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >"$WORK/xvfb.log" 2>&1 &
XPID=$!
CPID=
cleanup() {
    [ -n "$CPID" ] && kill "$CPID" 2>/dev/null || true
    kill "$XPID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

I=0
until timeout 2 xwininfo -root -display "$DISP" >/dev/null 2>&1; do
    I=$((I + 1))
    if [ "$I" -ge 100 ]; then
        echo "window: FAIL - Xvfb did not come up on $DISP" >&2
        cat "$WORK/xvfb.log" >&2
        exit 1
    fi
    sleep 0.1
done

dotnet build "$ROOT/client" -c Release >/dev/null

# check_filter <name> <extra client arguments...>
check_filter() {
    name=$1
    shift
    dir=$WORK/$name
    mkdir -p "$dir"

    DISPLAY=$DISP dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" \
        --file "$ROOT/tests/data/sample-video.bin" --stats 0 --hold "$@" \
        >"$dir/client.log" 2>&1 &
    CPID=$!

    # wait for the window (and then a moment for the first present)
    ID=
    I=0
    while [ -z "$ID" ] && [ "$I" -lt 100 ]; do
        ID=$(DISPLAY=$DISP xwininfo -root -tree 2>/dev/null |
             grep '"YT2 client"' | awk '{print $1}' | head -1)
        [ -n "$ID" ] || sleep 0.1
        I=$((I + 1))
    done
    if [ -z "$ID" ]; then
        echo "window: FAIL ($name) - the client never created a window titled 'YT2 client'" >&2
        cat "$dir/client.log" >&2
        exit 1
    fi

    INFO=$(DISPLAY=$DISP xwininfo -id "$ID" 2>&1)
    printf '%s\n' "$INFO" >"$dir/xwininfo.txt"

    if ! printf '%s\n' "$INFO" | grep -q "Map State: IsViewable"; then
        echo "window: FAIL ($name) - the window exists but is not mapped (hidden)" >&2
        printf '%s\n' "$INFO" | grep "Map State" >&2
        cat "$dir/client.log" >&2
        exit 1
    fi

    W=$(printf '%s\n' "$INFO" | awk '/Width:/{print $2}')
    H=$(printf '%s\n' "$INFO" | awk '/Height:/{print $2}')
    if [ "$W" != "960" ] || [ "$H" != "600" ]; then
        echo "window: FAIL ($name) - mapped window is ${W}x${H}, expected 960x600" >&2
        exit 1
    fi

    # the window appears before the recording is decoded; wait for the first
    # present instead of racing it
    I=0
    until grep -q "client: first frame shown 320x200" "$dir/client.log"; do
        I=$((I + 1))
        if [ "$I" -ge 150 ]; then
            echo "window: FAIL ($name) - the window never presented the sample frame" >&2
            cat "$dir/client.log" >&2
            exit 1
        fi
        sleep 0.1
    done

    kill "$CPID" 2>/dev/null || true
    wait "$CPID" 2>/dev/null || true
    CPID=
    echo "window: OK ($name, mapped ${W}x${H} on $DISP, frame presented)"
}

check_filter nearest
check_filter xbrz --filter xbrz

echo "window: artifacts in $WORK"
