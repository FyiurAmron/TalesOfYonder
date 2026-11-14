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
    WORLD="$ROOT/_ref/orig-bin/YT2/WORLD.DAT"
fi
if [ ! -f "$WORLD" ]; then
    WORLD=
fi

PICTURES="$ROOT/build/swreg/oracle/PICTURES.VGA"
if [ ! -f "$PICTURES" ]; then
    PICTURES="$ROOT/_ref/orig-bin/YT2/PICTURES.VGA"
fi
if [ ! -f "$PICTURES" ]; then
    PICTURES=
fi

mkdir -p "$RUN/unit-tmp"
YT2_SAVE="$SAVE" YT2_SAVE_ROUNDTRIP="$RUN/roundtrip.save" \
    YT2_SAVE_MOVED="$RUN/moved.save" YT2_WORLD="$WORLD" \
    YT2_PICTURES="$PICTURES" YT2_PORT_TMP="$RUN/unit-tmp" \
    "$BUILD/yt2p-tests" >"$RUN/tests.log"
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

    # The oracle frame carries the hand cursor at (24,170); this hash is the
    # page composition before the display layer draws the pointer.
    "$BUILD/yt2p-cluebook" "$PICTURES" "$WORLD" "$RUN/cluebook.ppm" \
        >"$RUN/cluebook.txt"
    if ! grep -q \
            "cluebook: frame 07D368FE31D7447F palette-fnv E3275EE3AE459FB5" \
            "$RUN/cluebook.txt"; then
        echo "port: clue-book page does not match the original" >&2
        cat "$RUN/cluebook.txt" >&2
        exit 1
    fi
    if [ "$(wc -c <"$RUN/cluebook.ppm")" -ne 192015 ]; then
        echo "port: clue-book PPM has the wrong size" >&2
        exit 1
    fi
    echo "port: clue-book page composes to the original frame"

    # The oracle frame carries the hand cursor at (24,170); the catalog hash is
    # the page composition before the display layer draws the pointer.
    mkdir -p "$RUN/catalog"
    "$BUILD/yt2p-catalog" "$PICTURES" "$WORLD" 67 "$RUN/catalog" \
        >"$RUN/catalog.txt"
    if ! grep -q "catalog 67 rank 0 frame 064C1427810F47E3" \
            "$RUN/catalog.txt"; then
        echo "port: clue-book map catalog does not match the original" >&2
        cat "$RUN/catalog.txt" >&2
        exit 1
    fi
    "$BUILD/yt2p-mapview" "$PICTURES" "$WORLD" 67 "$RUN/catalog" \
        >"$RUN/mapview.txt"
    if ! grep -q \
            "map 67 ANCIENT RUINS            frame 725194639EB33B4F palette E3275EE3AE459FB5" \
            "$RUN/mapview.txt"; then
        echo "port: overhead map does not compose the intended frame" >&2
        cat "$RUN/mapview.txt" >&2
        exit 1
    fi
    echo "port: clue-book catalog and overhead map compose"

    # Both oracle frames carry the hand cursor at (24,170); the hashes are
    # the page compositions before the display layer draws the pointer.
    mkdir -p "$RUN/monsters"
    "$BUILD/yt2p-monsters" "$PICTURES" "$WORLD" 0 "$RUN/monsters" \
        >"$RUN/monsters.txt"
    "$BUILD/yt2p-monsters" "$PICTURES" "$WORLD" 60 "$RUN/monsters" \
        >>"$RUN/monsters.txt"
    if ! grep -q "monsters 0 frame D5B4C34273DCD369" "$RUN/monsters.txt" ||
       ! grep -q "monsters 60 frame 6E3F491525EA591E" "$RUN/monsters.txt"; then
        echo "port: monster statistics do not match the original" >&2
        cat "$RUN/monsters.txt" >&2
        exit 1
    fi
    echo "port: monster statistics compose"

    # The oracle frames carry the hand cursor at (24,170); these hashes are
    # the page compositions before the display layer draws the pointer.
    # Rank 98's page turn presents a transitional frame first; the pinned
    # composition matches the settled one (plus the cursor, in test_cursor).
    mkdir -p "$RUN/spells"
    "$BUILD/yt2p-spells" "$PICTURES" "$WORLD" 0 "$RUN/spells" \
        >"$RUN/spells.txt"
    "$BUILD/yt2p-spells" "$PICTURES" "$WORLD" 14 "$RUN/spells" \
        >>"$RUN/spells.txt"
    "$BUILD/yt2p-spells" "$PICTURES" "$WORLD" 98 "$RUN/spells" \
        >>"$RUN/spells.txt"
    "$BUILD/yt2p-spells" "$PICTURES" "$WORLD" 104 "$RUN/spells" \
        >>"$RUN/spells.txt"
    if ! grep -q "spells 0 frame 336AED2DF4355F25" "$RUN/spells.txt" ||
       ! grep -q "spells 14 frame 4E0021E2C76A7281" "$RUN/spells.txt" ||
       ! grep -q "spells 98 frame 14A80E5E78EF674E" "$RUN/spells.txt" ||
       ! grep -q "spells 104 frame EB4FF720BE73034E" "$RUN/spells.txt"; then
        echo "port: spell information does not match the original" >&2
        cat "$RUN/spells.txt" >&2
        exit 1
    fi
    for detail in 0 3 31 60 85 101 104; do
        "$BUILD/yt2p-spells" "$PICTURES" "$WORLD" detail "$detail" \
            "$RUN/spells" >>"$RUN/spells.txt"
    done
    "$BUILD/yt2p-spells" "$PICTURES" "$WORLD" detail-index 86 \
        "$RUN/spells" >>"$RUN/spells.txt"
    cat >"$RUN/spells-detail-expected.txt" <<'EOF'
spells detail 0 frame 0FB79595E489737C
spells detail 3 frame 23A5CEF0412FAD86
spells detail 31 frame AC8DE7BF1FEF84C6
spells detail 60 frame 0A7260957E4324FD
spells detail 85 frame DE7BEED456D5549F
spells detail 101 frame 891A56D3D6EAEF4F
spells detail 104 frame 3B7482191D934F83
spells detail-index 86 frame 6E6CF078435509A7
EOF
    if ! grep -E "spells detail(-index)?" "$RUN/spells.txt" | \
            diff -u "$RUN/spells-detail-expected.txt" -; then
        echo "port: spell details do not match the original" >&2
        exit 1
    fi
    echo "port: spell information composes"

    # The F4 class list (no MORE line on its single page) and the six
    # spell-by-class details; the oracle frames carry the hand at (24,170)
    # and the pinned compositions match them in test_cursor.
    mkdir -p "$RUN/classes"
    "$BUILD/yt2p-classes" "$PICTURES" "$WORLD" list 0 "$RUN/classes" \
        >"$RUN/classes.txt"
    "$BUILD/yt2p-classes" "$PICTURES" "$WORLD" list 5 "$RUN/classes" \
        >>"$RUN/classes.txt"
    for spec in "0 0" "0 51" "1 0" "1 49" "2 0" "2 31" "3 0" "3 46" \
            "4 0" "4 46" "5 0" "5 30"; do
        # shellcheck disable=SC2086
        "$BUILD/yt2p-classes" "$PICTURES" "$WORLD" detail $spec \
            "$RUN/classes" >>"$RUN/classes.txt"
    done
    cat >"$RUN/classes-expected.txt" <<'EOF'
classes list 0 frame 635F5999F0C8700A
classes list 5 frame 3E56F34BA5FD770A
classes detail 0 0 frame 7D2F8F049CB7FC98
classes detail 0 51 frame F3BB3DD90AE90F86
classes detail 1 0 frame 63D575509257F536
classes detail 1 49 frame 4894CC8AA5C6573B
classes detail 2 0 frame 53D69E6B5ECC0E63
classes detail 2 31 frame 22F3C1CCB32DFCAB
classes detail 3 0 frame 3E269B1BDE50C254
classes detail 3 46 frame 2997949A89D11BF3
classes detail 4 0 frame 9A05102B5253BD5E
classes detail 4 46 frame 481A7B87A5FBB989
classes detail 5 0 frame C2D26B9AA8411307
classes detail 5 30 frame DFA9A774EC40F77A
EOF
    if ! diff -u "$RUN/classes-expected.txt" "$RUN/classes.txt"; then
        echo "port: magic users do not match the original" >&2
        exit 1
    fi
    echo "port: magic users compose"

    # The F5 category list and the six item details; the oracle frames
    # carry the hand at (24,170) and the pinned compositions match them
    # in test_cursor.
    mkdir -p "$RUN/items"
    "$BUILD/yt2p-items" "$PICTURES" "$WORLD" list 0 "$RUN/items" \
        >"$RUN/items.txt"
    "$BUILD/yt2p-items" "$PICTURES" "$WORLD" list 7 "$RUN/items" \
        >>"$RUN/items.txt"
    for spec in "0 0" "0 40" "2 0" "2 51" "3 0" "3 42" "4 0" "4 12" \
            "5 0" "5 26" "7 0" "7 23"; do
        # shellcheck disable=SC2086
        "$BUILD/yt2p-items" "$PICTURES" "$WORLD" detail $spec \
            "$RUN/items" >>"$RUN/items.txt"
    done
    "$BUILD/yt2p-items" "$PICTURES" "$WORLD" attribute "$RUN/items" \
        >>"$RUN/items.txt"
    "$BUILD/yt2p-items" "$PICTURES" "$WORLD" transport "$RUN/items" \
        >>"$RUN/items.txt"
    for spec in "0 0 0" "0 8 0" "2 0 0" "3 27 0" "4 0 0" "4 3 0" "5 9 0" \
            "7 15 0" "0 0 5"; do
        # shellcheck disable=SC2086
        "$BUILD/yt2p-items" "$PICTURES" "$WORLD" page $spec \
            "$RUN/items" >>"$RUN/items.txt"
    done
    cat >"$RUN/items-expected.txt" <<'EOF'
items list 0 frame 52651339B2A27723
items list 7 frame 4A2B69FF4D3178A3
items detail 0 0 frame C87C5DD07A185A62
items detail 0 40 frame D92112F791E7D764
items detail 2 0 frame 1E817B5AF150837B
items detail 2 51 frame B6584121446C9BE7
items detail 3 0 frame 59C84B0330566BC2
items detail 3 42 frame C28B92EDC54E105C
items detail 4 0 frame 058C34726C803E2C
items detail 4 12 frame 7FF1158C5AE3F7AC
items detail 5 0 frame A6FCF8E8B1E8075E
items detail 5 26 frame 93B6F804E0C2FF14
items detail 7 0 frame E67074CED9AF3D42
items detail 7 23 frame 381074A149039DDA
items attribute frame 95390B103B037E48
items transport frame 65F8EE40B9331EF8
items page 0 0 +0 frame B3D20242B09338EB
items page 0 8 +0 frame 09261DA6156A2B77
items page 2 0 +0 frame 5D12B0CDD1A500D2
items page 3 27 +0 frame C9335F4118960E4E
items page 4 0 +0 frame 3E9D8067847AC596
items page 4 3 +0 frame 5CB36EB9E65878A7
items page 5 9 +0 frame A3FA36F51F77EE9B
items page 7 15 +0 frame 46D80760FA64D742
items page 0 0 +5 frame B7513842755800DB
EOF
    if ! diff -u "$RUN/items-expected.txt" "$RUN/items.txt"; then
        echo "port: inventory items do not match the original" >&2
        exit 1
    fi
    echo "port: inventory items compose"

    # The F6 walkthrough pages; the oracle frames carry the hand at
    # (24,170) and the pinned compositions match them in test_cursor.
    # Pages 0-1 also match B1's own cursor-free ESC transitionals.
    mkdir -p "$RUN/walkthrough"
    for page in 0 1 14 19 30; do
        "$BUILD/yt2p-walkthrough" "$PICTURES" "$WORLD" "$page" \
            "$RUN/walkthrough" >>"$RUN/walkthrough.txt"
    done
    cat >"$RUN/walkthrough-expected.txt" <<'EOF'
walkthrough 0 frame 2006DFD600464B52
walkthrough 1 frame 22F421C5C49220A1
walkthrough 14 frame A2B9C0771EAF4A5D
walkthrough 19 frame 964B04246F019CFC
walkthrough 30 frame 8A6765000C222B95
EOF
    if ! diff -u "$RUN/walkthrough-expected.txt" "$RUN/walkthrough.txt"; then
        echo "port: walkthrough does not match the original" >&2
        exit 1
    fi
    echo "port: walkthrough composes"

    mkdir -p "$RUN/boot"
    "$BUILD/yt2p-boot" "$PICTURES" "$WORLD" "$RUN/boot" >"$RUN/boot.txt"
    cat >"$RUN/boot-expected.txt" <<'EOF'
scene webfoot         frame C5B1519EF4C1133A palette 089875EBAB4D4C6F
scene story-sleep     frame FB816B027A864AD1 palette 16E60732045316F6
scene story-knock     frame 033C9733AF00EB97 palette 16E60732045316F6
scene attract-combat  frame 1B323B732AF8CFD9 palette E3275EE3AE459FB5
scene attract-magic   frame E7BF3FF049508B10 palette E3275EE3AE459FB5
scene attract-dragons frame FD9A4B6D3EA90207 palette E3275EE3AE459FB5
scene attract-tower   frame 0384E6A33033B700 palette E3275EE3AE459FB5
scene attract-shop    frame EA4646AEFAF5EADD palette E3275EE3AE459FB5
scene title           frame 09BEC5E9162924EF palette E3275EE3AE459FB5
scene menu            frame 6ADE1397DAE95BC0 palette E3275EE3AE459FB5
EOF
    if ! diff -u "$RUN/boot-expected.txt" "$RUN/boot.txt"; then
        echo "port: start-up scenes do not match the original" >&2
        exit 1
    fi
    echo "port: start-up scenes compose to the original frames"

    # The engine speaks v0: the player presents the catalog with the hand at
    # the oracle capture's position, and the wire bytes decode to the oracle
    # framebuffer.
    mkdir -p "$RUN/play/game"
    cp "$PICTURES" "$RUN/play/game/PICTURES.VGA"
    cp "$WORLD" "$RUN/play/game/WORLD.DAT"
    "$BUILD/yt2p-play" "$RUN/play/game" --scene 1 --cursor 24,170 \
        --frames 3 --tick-ms 1 >"$RUN/play.txt" 2>&1
    if ! grep -q "play: 3 frames" "$RUN/play.txt"; then
        echo "port: yt2p-play did not present 3 frames" >&2
        cat "$RUN/play.txt" >&2
        exit 1
    fi
    if ! python3 "$PORT/tests/check_stream.py" "$RUN/play/game/OUTPUT.BIN" \
            BC1E5BFF608CB41B 3 >"$RUN/stream.txt" 2>&1; then
        echo "port: B2 stream does not decode to the oracle frame" >&2
        cat "$RUN/stream.txt" >&2
        exit 1
    fi
    echo "port: player stream decodes to the oracle framebuffer"
    if command -v dotnet >/dev/null 2>&1; then
        if ! sh "$ROOT/tests/replay/run.sh" "$RUN/play/game/OUTPUT.BIN" \
                >"$RUN/replay.txt" 2>&1; then
            echo "port: client and Python disagree on B2 output" >&2
            cat "$RUN/replay.txt" >&2
            exit 1
        fi
        echo "port: client renders B2 output like the reference decoder"
    else
        echo "port: dotnet missing; skipping the client render check"
    fi
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
