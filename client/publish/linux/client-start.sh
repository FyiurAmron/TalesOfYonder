#!/bin/sh
# Follows the running server's OUTPUT.BIN and sends input through INPUT.BIN.
# The client asks for a keyframe when it connects, so the start order does not
# matter; if the server is not up yet the client waits for the file.
set -e
cd "$(dirname "$0")"

if [ ! -x client/Yt2Client ]; then
    echo "client-start: client/Yt2Client is missing; unpack the bundle again."
    exit 1
fi
if [ ! -f server/game/SWREG.EXE ] && [ ! -x server/game/yt2game ]; then
    echo "client-start: server/game is missing; unpack the bundle again."
    exit 1
fi

exec client/Yt2Client --follow server/game/OUTPUT.BIN \
                      --input server/game/INPUT.BIN "$@"
