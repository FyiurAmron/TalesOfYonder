#!/bin/sh
# Run the seam build (serial backend) under DOSBox-X with COM1 bridged to TCP.
#
# OBSOLETE: the folder link (build/swreg/file) is the transport the product
# uses - the client follows STREAM.BIN and writes INPUT.BIN, and a shared
# directory carries that across a machine boundary just as well.  This script
# drives the serial link, which is kept only for fun.
#
#   client/server.sh                # build the seam, start the game window
#   client/server.sh --no-build     # use the existing build/swreg/serial/SWREG.EXE
#   client/server.sh --client       # also start the C# client
#
# The game window is DOSBox-X; the client connects to 127.0.0.1:2324 (see
# tools/dosbox/dosbox-x-client.conf).  In another terminal:
#
#   dotnet run --project client -c Release
#
# The client asks for a keyframe when it connects, so the start order does not
# matter.  See docs/client.md.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUN="$ROOT/build/runs/server"
BUILD=1
CLIENT=0

usage() {
    echo "usage: client/server.sh [--no-build] [--client]" >&2
}

for arg in "$@"; do
    case "$arg" in
        --no-build) BUILD=0 ;;
        --client) CLIENT=1 ;;
        -h|--help) usage; exit 0 ;;
        *) usage; exit 2 ;;
    esac
done

if [ "$BUILD" = 1 ]; then
    sh "$ROOT/engine/asm/swreg/tools/build.sh" --variant serial
fi

mkdir -p "$RUN/game" "$RUN/capture"
for f in SWREG.EXE CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp -f "$ROOT/build/swreg/serial/$f" "$RUN/game/"
done

cd "$RUN"
echo "server: game in $RUN/game"
echo "server: COM1 is a nullmodem TCP server on 127.0.0.1:2324"

CLIENT_PID=
if [ "$CLIENT" = 1 ]; then
    dotnet build "$ROOT/client" -c Release >/dev/null
    dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" --tcp 127.0.0.1:2324 &
    CLIENT_PID=$!
fi

dosbox-x -conf "$ROOT/tools/dosbox/dosbox-x-client.conf"

if [ -n "$CLIENT_PID" ]; then
    kill "$CLIENT_PID" 2>/dev/null || true
fi
