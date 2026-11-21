#!/bin/sh
# Starts the B2 port player (yt2p-play) with the folder link.
#
# The link is the folder in server/game: the engine writes OUTPUT.BIN and
# reads INPUT.BIN, and client-start.sh follows them.  No DOSBox-X and no
# emulation is involved: the player composes screens with the hand-written
# port engine and presents them over protocol v0.  Extra arguments pass
# through to the player (see --scene, --cursor, --tick-ms).
set -e
cd "$(dirname "$0")/server"

if [ ! -x game/yt2p-play ]; then
    echo "server-start: server/game/yt2p-play is missing; unpack the bundle again."
    exit 1
fi

echo "server-start: starting the B2 player (folder link)"
# Port bundles run enhanced (resume past the intro when prior state
# exists); unset for a vanilla boot.
export YT2_MODE=enhanced
exec ./game/yt2p-play game "$@"
