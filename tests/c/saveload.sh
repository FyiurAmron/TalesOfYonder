#!/bin/sh
# Save/load gate: run the M5 opening, then drive the game's disk panel --
# save the chest-loot state to slot 1, walk four steps back and save the
# changed state to slot 2, load slot 1 back and save the restored state to
# slot 3.  The checker requires the walk to show up in slot 2, the load to
# restore the whole party blob (slot 3 matches slot 1 from the first party
# record on) and the live DGROUP position to match after the load.
#
#   sh tests/c/saveload.sh              # build if needed
#   sh tests/c/saveload.sh --no-build   # use build/c/yt2game as it is
#
# The input script is tests/c/m5_input.txt plus the disk-panel keys appended
# below, so the M5 route stays the single source of truth for the opening.
# Artifacts land in build/runs/saveload/<stamp>/.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/saveload/$STAMP"
GAME="$BUILD/yt2game"
TICKS=9000
TRANSFERS=500000000
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "saveload: unknown option $1" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN/game" "$RUN/dump"
if [ "$DO_BUILD" = yes ] || [ ! -x "$GAME" ] ||
   [ ! -x "$BUILD/yt2saveinfo" ] ||
   [ -n "$(find "$ROOT/engine/c/src" "$ROOT/engine/c/include" \
                 "$ROOT/engine/asm/swreg/src" "$ROOT/tools/port" \
                 "$ROOT/engine/c/Makefile" "$ROOT/engine/c/tools/build.sh" \
                 -newer "$GAME" -print -quit 2>/dev/null)" ]; then
    if ! sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1; then
        cat "$RUN/build.log" >&2
        echo "saveload: build failed" >&2
        exit 1
    fi
fi

for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/YT2/$file" "$RUN/game/$file"
done

{
    cat "$ROOT/tests/c/m5_input.txt"
    cat "$ROOT/tests/c/saveload_keys.txt"
} >"$RUN/input.txt"

set +e
# No sound keeps the scripted ticks identical to the M5 gate; the fixed DOS
# time pins the clock-seeded RNG.
timeout 300s "$GAME" --game "$RUN/game" --link --ticks "$TICKS" --no-sound \
    --transfers "$TRANSFERS" --pin-dos-time 0x1234 \
    --input-script "$RUN/input.txt" \
    --present-log "$RUN/present.txt" --dump "$RUN/dump" \
    >"$RUN/stdout.log" 2>"$RUN/stderr.log"
STATUS=$?
set -e
if [ "$STATUS" -ne 0 ]; then
    cat "$RUN/stderr.log" >&2
    echo "saveload: runner failed with status $STATUS" >&2
    exit 1
fi
if ! grep -q 'exited=0' "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "saveload: the game exited early" >&2
    exit 1
fi

python3 "$ROOT/tests/c/saveload.py" "$RUN/dump" "$RUN/game"

# The C data model reads the same file the game wrote.  This pins the typed
# structs to the guest serialization, not just the Python checker's offsets.
"$BUILD/yt2saveinfo" "$RUN/game/SAVGAME1" >"$RUN/saveinfo.txt"
for line in \
    "party: position 00A1:0022 facing 4000 speed 5" \
    "slot 0: CONAN class 1 sex 1 age 19 level 0 weight 37 status 0000" \
    "  current: str 50 dex 49 sta 50 int 52 wis 60 cha 49" \
    "    proj 55/1 battle 54/3 absorb 0 hp 4 mp 0 carry 500" \
    "  maximum: str 50 dex 49 sta 50 int 52 wis 60 cha 49" \
    "    proj 55/1 battle 54/3 absorb 0 hp 13 mp 0 carry 500" \
    "  gear: carried 19 flags 0880" \
    "slot 5: SQUIRE class 2 sex 1 age 33 level 14 weight 37 status 0000"
do
    if ! grep -q "^$line" "$RUN/saveinfo.txt"; then
        cat "$RUN/saveinfo.txt" >&2
        echo "saveload: yt2saveinfo did not report '$line'" >&2
        exit 1
    fi
done

echo "saveload: OK (artifacts in $RUN)"
