#!/bin/sh
# Play the transliterated C engine against the C# client, from the tree.
#
#   sh tools/play-c.sh                  # client window, scaled 3x
#   sh tools/play-c.sh --scale 4        # options go to the client
#   sh tools/play-c.sh --headless --stats 5
#   sh tools/play-c.sh --spelltext      # YT3-sourced F3 descriptions (92)
#   sh tools/play-c.sh --spelltext-complete  # all 105 (13 curated lines)
#   PLAY_WORLD=build/tmp/lab.DAT sh tools/play-c.sh  # custom WORLD.DAT
#
# PLAY_WORLD stages a different WORLD.DAT (e.g. tools/doctor_world.py
# output for spell experiments) instead of the reference one; it combines
# with --spelltext (the patch applies on top of the custom base).
# Builds yt2game when it is missing or older than the sources, stages the
# original game data in build/tmp/play/game, starts the engine with --live and
# then the client.  Closing the client (or Ctrl+Q) stops the engine too.
# The engine writes build/tmp/play/game/OUTPUT.BIN and reads INPUT.BIN; the
# client follows them through the normal folder link.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=$ROOT/build/tmp/play/game
ENGINE=$ROOT/build/c/yt2game

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

WORLD_SRC=${PLAY_WORLD:-$ROOT/_ref/orig-bin/YT2/WORLD.DAT}
if [ ! -f "$WORLD_SRC" ]; then
    echo "play-c: world $WORLD_SRC is missing" >&2
    exit 1
fi
mkdir -p "$GAME"
for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    src=$ROOT/_ref/orig-bin/YT2/$file
    if [ "$file" = WORLD.DAT ]; then
        src=$WORLD_SRC
    fi
    if [ ! -f "$src" ]; then
        echo "play-c: $src is missing" >&2
        exit 1
    fi
    if [ ! -f "$GAME/$file" ] || [ "$src" -nt "$GAME/$file" ]; then
        cp -f "$src" "$GAME/$file"
    fi
done
if [ -n "$SPELLTEXT" ]; then
    FLAG=
    if [ "$SPELLTEXT" = complete ]; then
        FLAG=--complete
    fi
    # shellcheck disable=SC2086
    python3 "$ROOT/tools/yt2_spelltext.py" \
        "$WORLD_SRC" \
        "$ROOT/_ref/orig-bin/YT3/WORLD.DAT" "$GAME/WORLD.DAT" $FLAG --check
else
    # WORLD.DAT is read-only to the game: always restore the base, so a
    # previous --spelltext run cannot leak into this one.
    cp -f "$WORLD_SRC" "$GAME/WORLD.DAT"
fi

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
