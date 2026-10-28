#!/bin/sh
# Play the transliterated C engine against the C# client, from the tree.
#
#   sh tools/play-c.sh                  # client window, scaled 3x
#   sh tools/play-c.sh --scale 4        # options go to the client
#   sh tools/play-c.sh --headless --stats 5
#
# Builds yt2game when it is missing or older than the sources, stages the
# original game data in build/tmp/play/game, starts the engine with --live and
# then the client.  Closing the client (or Ctrl+Q) stops the engine too.
# The engine writes build/tmp/play/game/OUTPUT.BIN and reads INPUT.BIN; the
# client follows them through the normal folder link.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=$ROOT/build/tmp/play/game
ENGINE=$ROOT/build/c/yt2game

mkdir -p "$GAME"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    if [ ! -f "$ROOT/_ref/orig-bin/$file" ]; then
        echo "play-c: _ref/orig-bin/$file is missing" >&2
        exit 1
    fi
    if [ ! -f "$GAME/$file" ] || [ "$ROOT/_ref/orig-bin/$file" -nt "$GAME/$file" ]; then
        cp -f "$ROOT/_ref/orig-bin/$file" "$GAME/$file"
    fi
done

if [ ! -x "$ENGINE" ] ||
   [ -n "$(find "$ROOT/engine/c/src" "$ROOT/engine/c/include" \
                 "$ROOT/engine/asm/swreg/src" "$ROOT/tools/port" \
                 "$ROOT/engine/c/Makefile" -newer "$ENGINE" -print -quit 2>/dev/null)" ]; then
    echo "play-c: building the engine"
    sh "$ROOT/engine/c/tools/build.sh" >"$ROOT/build/tmp/play/build.log" 2>&1 || {
        cat "$ROOT/build/tmp/play/build.log" >&2
        echo "play-c: build failed" >&2
        exit 1
    }
fi

echo "play-c: engine $ENGINE"
"$ENGINE" --game "$GAME" --live >"$ROOT/build/tmp/play/server.log" 2>&1 &
SERVER=$!
trap 'kill "$SERVER" 2>/dev/null || true' EXIT INT TERM

echo "play-c: link $GAME/OUTPUT.BIN + INPUT.BIN"
dotnet run --project "$ROOT/client" -c Release -- \
    --follow "$GAME/OUTPUT.BIN" --input "$GAME/INPUT.BIN" "$@"
