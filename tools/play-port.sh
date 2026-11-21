#!/bin/sh
# Daily driver for the B2 port: build the player, stage a game directory,
# start the player and open the client on the live link.  Extra arguments
# pass through to the client (e.g. --scale 4).
#
#   sh tools/play-port.sh [--spelltext | --spelltext-complete]
#
# --spelltext plays with YT3-sourced F3 detail descriptions (the official
# 92-spell table); --spelltext-complete fills all 105 (13 curated lines).
# Keys 1-6 switch scenes (F1-F6/arrows/PgDn/PgUp/Enter/ESC work as in the game),
# the mouse moves the hand cursor, Q quits the player.  Closing the client
# window stops the client; this script then stops the player.  For a
# redistributable bundle use tools/publish-linux.sh --server port instead.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PLAY="$ROOT/build/play"
CLIENT="$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll"

SPELLTEXT=
LEFT=$#
while [ "$LEFT" -gt 0 ]; do
    case "$1" in
        --spelltext) SPELLTEXT=official ;;
        --spelltext-complete) SPELLTEXT=complete ;;
        *) set -- "$@" "$1" ;;
    esac
    shift
    LEFT=$((LEFT - 1))
done

sh "$ROOT/engine/port/tools/build.sh"
dotnet build "$ROOT/client" -c Release >/dev/null

mkdir -p "$PLAY"
ln -sf "$ROOT/_ref/orig-bin/YT2/PICTURES.VGA" "$PLAY/"
if [ -n "$SPELLTEXT" ]; then
    FLAG=
    if [ "$SPELLTEXT" = complete ]; then
        FLAG=--complete
    fi
    # Never write through the vanilla symlink: drop it first.
    rm -f "$PLAY/WORLD.DAT"
    python3 "$ROOT/tools/yt2_spelltext.py" \
        "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" \
        "$ROOT/_ref/orig-bin/YT3/WORLD.DAT" "$PLAY/WORLD.DAT" $FLAG --check
    PLAYER_FLAGS=--spelltext
else
    ln -sf "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" "$PLAY/"
    PLAYER_FLAGS=
fi

# Transport files are per-session: drop stale ones so a previous run's
# bytes can never leak into this link (CURGAME stays: it is the save).
rm -f "$PLAY/INPUT.BIN" "$PLAY/OUTPUT.BIN"
# The daily driver runs enhanced (resume past the intro when prior
# state exists); unset for a vanilla boot.
export YT2_MODE=enhanced
# shellcheck disable=SC2086
"$ROOT/build/port/yt2p-play" "$PLAY" $PLAYER_FLAGS >"$PLAY/player.log" 2>&1 &
PLAYER=$!
trap 'kill "$PLAYER" "$CLIENT" 2>/dev/null || true' EXIT INT TERM
sleep 1

# Either side may exit first (window close stops the client, quit to
# DOS stops the player); stop the other instead of hanging. kill -0
# succeeds on the unreaped zombie, so read the state from /proc.
dotnet "$CLIENT" --follow "$PLAY/OUTPUT.BIN" --input "$PLAY/INPUT.BIN" "$@" &
CLIENT=$!
alive() {
    case $(awk '{print $3}' "/proc/$1/stat" 2>/dev/null) in
        R|S|D|T|t) return 0 ;;
        *) return 1 ;;
    esac
}
while alive "$PLAYER" && alive "$CLIENT"; do
    sleep 1
done
kill "$PLAYER" "$CLIENT" 2>/dev/null || true
wait 2>/dev/null || true
