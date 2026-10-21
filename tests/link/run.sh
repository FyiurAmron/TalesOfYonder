#!/bin/sh
# Live link check: engine (tee build) -> COM1 -> DOSBox-X -> TCP -> client.
#
# OBSOLETE: the folder link (build/swreg/file) is the transport the product
# uses, and the client follows the same files it would follow over the wire.
# This check drives the serial link, which is kept only for fun.
#
#   sh tests/link/run.sh [--seconds N] [--display :N] [--port N] [--no-build]
#                        [--key SECONDS:SCAN:ASCII ...]
#                        [--mouse SECONDS:DX:DY ...] [--click SECONDS:BUTTON:0|1 ...]
#
# Builds the tee variant (-DSEAM -DSEAM_FILE -DSEAM_TEE: the seam writes
# STREAM.BIN *and* mirrors every message to COM1), starts DOSBox-X headless on
# a private Xvfb display with COM1 bridged to TCP, runs the C# client with
# --dump, then verifies with tools/stream/compare_streams.py that every message the
# client received is byte-identical to the seam's own STREAM.BIN from the same
# session.  The messages sent before the client connected are reported but are
# not failures (the client asks for a keyframe and resynchronises).
#
# Needs dosbox-x, Xvfb and dotnet.  Artifacts: build/runs/link/<stamp>/.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
RUN_SECONDS=40
DISPLAY_NUM=:88
PORT=2324
DO_BUILD=1
KEYS=

while [ $# -gt 0 ]; do
    case "$1" in
        --seconds) RUN_SECONDS=$2; shift 2 ;;
        --display) DISPLAY_NUM=$2; shift 2 ;;
        --port) PORT=$2; shift 2 ;;
        --no-build) DO_BUILD=0; shift ;;
        --key) KEYS="$KEYS --key $2"; shift 2 ;;
        --mouse) KEYS="$KEYS --mouse $2"; shift 2 ;;
        --click) KEYS="$KEYS --click $2"; shift 2 ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "link: unknown option $1" >&2; exit 2 ;;
    esac
done

if [ "$DO_BUILD" = 1 ]; then
    sh "$ROOT/engine/asm/swreg/tools/build.sh" --variant tee
fi
BUILD=$ROOT/build/swreg/tee
for f in SWREG.EXE CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    if [ ! -f "$BUILD/$f" ]; then
        echo "link: $BUILD/$f is missing (build the tee variant first)" >&2
        exit 1
    fi
done

# a private display, so parallel runs do not collide
while [ -e "/tmp/.X11-unix/X${DISPLAY_NUM#:}" ]; do
    DISPLAY_NUM=":$(( ${DISPLAY_NUM#:} + 1 ))"
done

dotnet build "$ROOT/client" -c Release >/dev/null

# One session: the engine under DOSBox-X, the client against it, and the
# comparison of what the client received with the engine's own STREAM.BIN.
#
# The emulated serial link is lossy: when the host's TCP thread stalls, the
# emulator's nullmodem buffer overflows and a chunk of the stream is lost.
# That is the emulator, not the engine - the engine's file copy stays whole
# and the loss shows up as a gap in the received stream.  The session is
# therefore run twice when the first one shows such a loss, and the check
# only fails when both sessions lose data.
ATTEMPTS=2
attempt=1
while [ "$attempt" -le "$ATTEMPTS" ]; do
    WORK=$ROOT/build/runs/link/$(date +%Y%m%d-%H%M%S)-try$attempt
    rm -rf "$WORK"
    mkdir -p "$WORK/game" "$WORK/capture"
    for f in SWREG.EXE CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
        cp "$BUILD/$f" "$WORK/game/$f"
    done

    Xvfb "$DISPLAY_NUM" -screen 0 1024x768x24 -nolisten tcp >"$WORK/xvfb.log" 2>&1 &
    XVFB_PID=$!
    sleep 2

    cd "$WORK"
    DISPLAY=$DISPLAY_NUM SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
        dosbox-x -conf "$ROOT/tools/dosbox/dosbox-x-client.conf" \
        -set "serial serial1=nullmodem port:$PORT multiplier:16" \
        -set "dosbox captures=$WORK/capture" \
        -fastlaunch -time-limit "$RUN_SECONDS" >"$WORK/dosbox.log" 2>&1 &
    DOSBOX_PID=$!
    sleep 3

    dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" --tcp "127.0.0.1:$PORT" \
        --headless --stats 0 --connect-timeout 20 $KEYS \
        --ppm "$WORK/client.ppm" --dump "$WORK/client.dump" >"$WORK/client.log" 2>&1 || true
    wait $DOSBOX_PID 2>/dev/null || true
    kill $XVFB_PID 2>/dev/null || true
    wait $XVFB_PID 2>/dev/null || true
    cd "$ROOT"

    cat "$WORK/client.log"
    if [ ! -s "$WORK/client.dump" ]; then
        echo "link: FAIL - the client received nothing (see $WORK/dosbox.log)" >&2
        exit 1
    fi
    if python3 "$ROOT/tools/stream/compare_streams.py" "$WORK/game/STREAM.BIN" "$WORK/client.dump"; then
        echo "link: artifacts in $WORK"
        exit 0
    fi
    if [ "$attempt" -ge "$ATTEMPTS" ]; then
        echo "link: FAIL - the link lost data in $ATTEMPTS sessions" >&2
        exit 1
    fi
    echo "link: the emulated link lost data in this session, running it again" >&2
    attempt=$((attempt + 1))
done
