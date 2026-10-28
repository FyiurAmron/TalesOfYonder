#!/bin/sh
# Starts the C engine (yt2game) with the folder link.
#
# The link is the folder in server/game: the engine writes OUTPUT.BIN and
# reads INPUT.BIN, and client-start.sh follows them.  Unlike the asm build,
# no DOSBox-X is involved: yt2game runs the transliterated original natively
# and paces itself to the oracle's 30,000 cycles/ms.
set -e
cd "$(dirname "$0")/server"

if [ ! -x game/yt2game ]; then
    echo "server-start: server/game/yt2game is missing; unpack the bundle again."
    exit 1
fi

echo "server-start: starting the C engine (folder link)"
exec ./game/yt2game --game game --live
