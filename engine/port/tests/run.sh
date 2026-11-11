#!/bin/sh
# B2 real C port gate: build the hand-written engine slice, run its unit tests,
# and compare its save/party reader with the B1 executable spec on a real save.
#
#   sh engine/port/tests/run.sh [--no-build]
#
# Artifacts land in build/runs/port/<stamp>/.
set -eu
PORT=$(cd "$(dirname "$0")/.." && pwd)
ROOT=$(cd "$PORT/../.." && pwd)
BUILD="$ROOT/build/port"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/port/$STAMP"
DO_BUILD=yes

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        *)
            echo "port: unknown option $1" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN"
if [ "$DO_BUILD" = yes ]; then
    make -C "$PORT" BUILD="$BUILD" all
fi

SAVE=$(ls -dt "$ROOT"/build/runs/saveload/*/game/SAVGAME1 2>/dev/null | head -1)
if [ -z "$SAVE" ] || [ ! -f "$SAVE" ] || [ "$(wc -c <"$SAVE")" -ne 77509 ]; then
    SAVE=$(ls -dt "$ROOT"/build/runs/m5/*/game/CURGAME 2>/dev/null | head -1)
fi
if [ -z "$SAVE" ] || [ ! -f "$SAVE" ] || [ "$(wc -c <"$SAVE")" -ne 77509 ]; then
    SAVE=
fi

WORLD="$ROOT/build/swreg/oracle/WORLD.DAT"
if [ ! -f "$WORLD" ]; then
    WORLD="$ROOT/_ref/orig-bin/WORLD.DAT"
fi
if [ ! -f "$WORLD" ]; then
    WORLD=
fi

PICTURES="$ROOT/build/swreg/oracle/PICTURES.VGA"
if [ ! -f "$PICTURES" ]; then
    PICTURES="$ROOT/_ref/orig-bin/PICTURES.VGA"
fi
if [ ! -f "$PICTURES" ]; then
    PICTURES=
fi

YT2_SAVE="$SAVE" YT2_SAVE_ROUNDTRIP="$RUN/roundtrip.save" \
    YT2_SAVE_MOVED="$RUN/moved.save" YT2_WORLD="$WORLD" \
    YT2_PICTURES="$PICTURES" "$BUILD/yt2p-tests" >"$RUN/tests.log"
cat "$RUN/tests.log"

if [ -n "$WORLD" ]; then
    "$BUILD/yt2p-world" "$WORLD" >"$RUN/world.txt"
    if ! grep -q "^maps: 120 (filler 48, world 25, dungeon 47)$" \
            "$RUN/world.txt" ||
       ! grep -q '^map 21: suffix "01  " name 1 type 1 "YENDOR-PORT HOPE"$' \
            "$RUN/world.txt"; then
        echo "port: WORLD.DAT loader output is wrong" >&2
        exit 1
    fi
    echo "port: WORLD.DAT maps and names parsed"
fi

if [ -n "$PICTURES" ]; then
    "$BUILD/yt2p-pictures" "$PICTURES" >"$RUN/pictures.txt"
    if ! grep -q "^pictures: 13054330 bytes, 10 groups$" \
            "$RUN/pictures.txt" ||
       ! grep -q "^group 0: 318x198 x23 offset 00000000 size 1448172 " \
            "$RUN/pictures.txt" ||
       ! grep -q "^group 9: 8x8 x576 offset 00C6A17A size 36864 " \
            "$RUN/pictures.txt"; then
        echo "port: PICTURES.VGA loader output is wrong" >&2
        exit 1
    fi
    echo "port: PICTURES.VGA groups parsed"
fi

if [ -n "$PICTURES" ] && [ -n "$WORLD" ]; then
    "$BUILD/yt2p-render" "$PICTURES" "$WORLD" 0 22 0 "$RUN/title.ppm" 1 0 \
        >"$RUN/render.txt"
    if ! grep -q "frame 09BEC5E9162924EF palette-fnv E3275EE3AE459FB5" \
            "$RUN/render.txt"; then
        echo "port: title frame does not match the original" >&2
        cat "$RUN/render.txt" >&2
        exit 1
    fi
    if [ "$(wc -c <"$RUN/title.ppm")" -ne 192015 ]; then
        echo "port: title PPM has the wrong size" >&2
        exit 1
    fi
    echo "port: title picture renders to the original frame"

    "$BUILD/yt2p-render" "$PICTURES" "$WORLD" 0 2 0 "$RUN/menu.ppm" 1 1 \
        >"$RUN/menu.txt"
    if ! grep -q "frame 6ADE1397DAE95BC0 palette-fnv E3275EE3AE459FB5" \
            "$RUN/menu.txt"; then
        echo "port: main-menu frame does not match the original" >&2
        cat "$RUN/menu.txt" >&2
        exit 1
    fi
    echo "port: main-menu picture renders to the original frame"
fi

if [ -z "$SAVE" ]; then
    echo "port: no 77509-byte save yet; run tests/c/saveload.sh first"
    echo "port: OK (unit tests only; artifacts in $RUN)"
    exit 0
fi

"$BUILD/yt2p-info" "$SAVE" >"$RUN/port.txt"
if [ -x "$ROOT/build/c/yt2saveinfo" ]; then
    "$ROOT/build/c/yt2saveinfo" "$SAVE" >"$RUN/b1.txt"
    if ! cmp -s "$RUN/b1.txt" "$RUN/port.txt"; then
        echo "port: save/party output differs from B1" >&2
        diff -u "$RUN/b1.txt" "$RUN/port.txt" >&2 || true
        exit 1
    fi
    echo "port: save/party output matches B1"
else
    if ! grep -q "slot 0: CONAN class 1 sex 1 age 19 level 0 weight 37 status 0000" \
            "$RUN/port.txt"; then
        echo "port: save/party output lost CONAN" >&2
        exit 1
    fi
    echo "port: save/party output parsed (B1 yt2saveinfo not built)"
fi

if ! cmp -s "$SAVE" "$RUN/roundtrip.save"; then
    echo "port: save round trip is not byte-identical" >&2
    cmp -l "$SAVE" "$RUN/roundtrip.save" | head -20 >&2 || true
    exit 1
fi
echo "port: save round trip is byte-identical"

if [ -x "$ROOT/build/c/yt2saveinfo" ]; then
    "$ROOT/build/c/yt2saveinfo" "$RUN/moved.save" >"$RUN/b1-moved.txt"
    if ! grep -q "^party: position 1234:5678 " "$RUN/b1-moved.txt"; then
        echo "port: moved save did not reach B1 with the new position" >&2
        exit 1
    fi
    sed 's/^party: position [0-9A-F]*:[0-9A-F]*/party: position 1234:5678/' \
        "$RUN/b1.txt" >"$RUN/b1-expected.txt"
    if ! diff -u "$RUN/b1-expected.txt" "$RUN/b1-moved.txt"; then
        echo "port: moved save changed more than the party position" >&2
        exit 1
    fi
    changed=$(cmp -l "$SAVE" "$RUN/moved.save" | awk '{print $1}' |
        tr '\n' ' ')
    if [ "$changed" != "153 154 155 156 " ]; then
        echo "port: moved save changed unexpected bytes: $changed" >&2
        exit 1
    fi
    echo "port: writer applies live state and preserves everything else"
fi

echo "port: OK (artifacts in $RUN)"
