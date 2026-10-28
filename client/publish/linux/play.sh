#!/bin/sh
# Starts the engine server (DOSBox-X for the asm build, yt2game for the C
# build) and then the client, for a one-command test.
#
#   ./play.sh            normal window
#   ./play.sh --scale 4  four host pixels per game pixel (default 3)
#
# Closing the client window stops the client; this script then stops the
# server it started.  Use server-start.sh + client-start.sh separately if you
# want to watch either side's console output.
set -e
cd "$(dirname "$0")"

./server-start.sh >server/start-server.log 2>&1 &
SERVER=$!
trap 'kill "$SERVER" 2>/dev/null || true' EXIT INT TERM

# give the server time to create OUTPUT.BIN; the client waits for it anyway
sleep 2

./client-start.sh "$@"
