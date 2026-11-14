#!/bin/sh
# Daily driver for the B2 port: build the player, stage a game directory,
# start the player and open the client on the live link.  Extra arguments
# pass through to the client (e.g. --scale 4).
#
#   sh tools/play-port.sh
#
# Keys 1-6 switch scenes (F1-F6/arrows/PgDn/PgUp/Enter/ESC work as in the game),
# the mouse moves the hand cursor, Q quits the player.  Closing the client
# window stops the client; this script then stops the player.  For a
# redistributable bundle use tools/publish-linux.sh --server port instead.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PLAY="$ROOT/build/play"
CLIENT="$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll"

sh "$ROOT/engine/port/tools/build.sh"
dotnet build "$ROOT/client" -c Release >/dev/null

mkdir -p "$PLAY"
ln -sf "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" "$ROOT/_ref/orig-bin/YT2/PICTURES.VGA" \
    "$PLAY/"

"$ROOT/build/port/yt2p-play" "$PLAY" >"$PLAY/player.log" 2>&1 &
PLAYER=$!
trap 'kill "$PLAYER" 2>/dev/null || true' EXIT INT TERM
sleep 1

dotnet "$CLIENT" --follow "$PLAY/OUTPUT.BIN" --input "$PLAY/INPUT.BIN" "$@"
