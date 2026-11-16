#!/bin/sh
# Starts the engine server (DOSBox-X for the asm build, yt2game for the C
# build, yt2p-play for the B2 port build) and then the client, for a
# one-command test.
#
#   ./play.sh            normal window
#   ./play.sh --scale 4  four host pixels per game pixel (default 3)
#
# Closing the client window stops the client; this script then stops the
# server it started, and if the server exits first (e.g. quit to DOS)
# the client is stopped too instead of following a dead link forever.
# Use server-start.sh + client-start.sh separately if you
# want to watch either side's console output.
set -e
cd "$(dirname "$0")"

# Fresh link per run: stale INPUT replays the previous session's keys
# (a leftover quit exits the boot), stale OUTPUT replays old frames.
rm -f server/game/INPUT.BIN server/game/OUTPUT.BIN

./server-start.sh >server/start-server.log 2>&1 &
SERVER=$!
trap 'kill "$SERVER" "$CLIENT" 2>/dev/null || true' EXIT INT TERM

# give the server time to create OUTPUT.BIN; the client waits for it anyway
sleep 2

./client-start.sh "$@" &
CLIENT=$!
# Either side may exit first (window close stops the client, quit to
# DOS stops the server); stop the other instead of hanging. kill -0
# succeeds on the unreaped zombie, so read the state from /proc.
alive() {
    case $(awk '{print $3}' "/proc/$1/stat" 2>/dev/null) in
        R|S|D|T|t) return 0 ;;
        *) return 1 ;;
    esac
}
while alive "$SERVER" && alive "$CLIENT"; do
    sleep 1
done
kill "$SERVER" "$CLIENT" 2>/dev/null || true
wait 2>/dev/null || true
