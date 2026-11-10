#!/bin/sh
# Cross-build the C engine for Windows x64 with MinGW-w64 and run it under
# Wine.  This gate is what makes "compiles and behaves the same on Windows"
# checkable from the Linux tree: the PE32+ images must pass the unit tests,
# and a deterministic game slice must produce byte-identical stdout, stderr,
# audio log, present log and checkpoint dump on both platforms.
#
#   sh tests/c/windows.sh [--no-build] [--full]
#
#   --full  also run the whole M5 + save/load script through both binaries and
#           compare SAVGAME1..3 and the end-of-run dump.
#
# Needs x86_64-w64-mingw32-gcc (gcc-mingw-w64-x86-64) and wine on PATH.  The
# gate is skipped with a note when either is missing, so the main C gate stays
# runnable on machines without a Windows toolchain.  Artifacts land in
# build/runs/windows/<stamp>/.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
WIN="$ROOT/build/c-win"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/windows/$STAMP"
DO_BUILD=auto
FULL=0

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --full) FULL=1; shift ;;
        *)
            echo "windows: unknown option $1" >&2
            exit 2
            ;;
    esac
done

if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "windows: skipped (x86_64-w64-mingw32-gcc is not on PATH)"
    echo "windows: install gcc-mingw-w64-x86-64, or set CC to a cross compiler"
    exit 0
fi
if ! command -v wine >/dev/null 2>&1; then
    echo "windows: skipped (wine is not on PATH)"
    exit 0
fi

if [ ! -x "$BUILD/yt2game" ]; then
    echo "windows: $BUILD/yt2game is missing; run tests/c/run.sh or engine/c/tools/build.sh" >&2
    exit 1
fi
if ! file "$BUILD/yt2game" | grep -q 'ELF 64-bit.*x86-64'; then
    echo "windows: $BUILD/yt2game is not a 64-bit x86-64 ELF image" >&2
    file "$BUILD/yt2game" >&2
    exit 1
fi

mkdir -p "$RUN"
if [ "$DO_BUILD" = yes ] || [ "$DO_BUILD" = auto ] || [ ! -x "$WIN/yt2game.exe" ]; then
    if ! sh "$ROOT/engine/c/tools/build-mingw.sh" >"$RUN/build.log" 2>&1; then
        cat "$RUN/build.log" >&2
        echo "windows: cross-build failed" >&2
        exit 1
    fi
fi

for program in yt2game yt2stub yt2core-tests yt2game-tests yt2saveinfo; do
    if [ ! -x "$WIN/$program.exe" ]; then
        echo "windows: $WIN/$program.exe is missing" >&2
        exit 1
    fi
    if ! file "$WIN/$program.exe" | grep -q 'PE32+.*x86-64'; then
        echo "windows: $WIN/$program.exe is not a 64-bit PE image" >&2
        file "$WIN/$program.exe" >&2
        exit 1
    fi
done
echo "windows: five PE32+ x86-64 programs"

export WINEDEBUG=-all
# Make sure the Wine prefix exists before the compared runs, so its first-use
# messages cannot end up in one platform's log but not the other's.
wine cmd /c exit >/dev/null 2>&1 || true

echo "windows: unit tests"
wine "$WIN/yt2core-tests.exe" | tee "$RUN/core.log"
wine "$WIN/yt2game-tests.exe" | tee "$RUN/game-unit.log"

mkdir -p "$RUN/lin/game" "$RUN/lin/dump" "$RUN/win/game" "$RUN/win/dump"
for side in lin win; do
    for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
        cp "$ROOT/_ref/orig-bin/$file" "$RUN/$side/game/$file"
    done
done

# One deterministic slice: the game clock pin fixes the RNG, --no-sound keeps
# the tick budget identical, and the logs/dump record everything the run did.
# The Linux and Windows binaries must agree byte for byte.
for side in lin win; do
    if [ "$side" = lin ]; then
        GAME="$BUILD/yt2game"
    else
        GAME="$WIN/yt2game.exe"
    fi
    set +e
    (
        cd "$RUN"
        if [ "$side" = lin ]; then
            timeout 120s "$GAME" --game "$side/game" --ticks 700 \
                --no-sound --transfers 500000000 \
                --audio-log "$side/audio.log" \
                --present-log "$side/present.txt" \
                --dump "$side/dump" >"$side/stdout.log" 2>"$side/stderr.log"
        else
            timeout 120s wine "$GAME" --game "$side/game" --ticks 700 \
                --no-sound --transfers 500000000 \
                --audio-log "$side/audio.log" \
                --present-log "$side/present.txt" \
                --dump "$side/dump" >"$side/stdout.log" 2>"$side/stderr.log"
        fi
    )
    STATUS=$?
    set -e
    if [ "$STATUS" -ne 0 ]; then
        cat "$RUN/$side/stderr.log" >&2
        echo "windows: $side run failed with status $STATUS" >&2
        exit 1
    fi
done

for file in stdout.log stderr.log audio.log present.txt; do
    if ! cmp -s "$RUN/lin/$file" "$RUN/win/$file"; then
        echo "windows: $file differs between Linux and Windows" >&2
        cmp -l "$RUN/lin/$file" "$RUN/win/$file" | head -10 >&2 || true
        exit 1
    fi
done
if ! diff -r "$RUN/lin/dump" "$RUN/win/dump" >"$RUN/dump.diff" 2>&1; then
    cat "$RUN/dump.diff" >&2
    echo "windows: the checkpoint dump differs between Linux and Windows" >&2
    exit 1
fi
echo "windows: 700-tick logs and checkpoint dump match"

# The typed save reader is the same C on both platforms; run it on a real
# save.  A prior M5 or save/load gate leaves one; otherwise just say so.
SAVE=""
for candidate in "$ROOT"/build/runs/saveload/*/game/SAVGAME1 \
                 "$ROOT"/build/runs/m5/*/game/CURGAME; do
    if [ -s "$candidate" ] && [ "$(wc -c <"$candidate")" -eq 77509 ]; then
        SAVE="$candidate"
    fi
done
if [ -n "$SAVE" ]; then
    mkdir -p "$RUN/save"
    cp "$SAVE" "$RUN/save/SAVGAME1"
    "$BUILD/yt2saveinfo" "$RUN/save/SAVGAME1" >"$RUN/saveinfo-lin.txt"
    (
        cd "$RUN"
        wine "$WIN/yt2saveinfo.exe" save/SAVGAME1
    ) >"$RUN/saveinfo-win.txt"
    if ! cmp -s "$RUN/saveinfo-lin.txt" "$RUN/saveinfo-win.txt"; then
        echo "windows: yt2saveinfo output differs between Linux and Windows" >&2
        diff -u "$RUN/saveinfo-lin.txt" "$RUN/saveinfo-win.txt" >&2 || true
        exit 1
    fi
    echo "windows: typed save reader matches"
else
    echo "windows: no save file yet; run tests/c/saveload.sh to exercise yt2saveinfo"
fi

if [ "$FULL" = 1 ]; then
    mkdir -p "$RUN/full/lin/game" "$RUN/full/lin/dump" \
             "$RUN/full/win/game" "$RUN/full/win/dump"
    for side in lin win; do
        for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
            cp "$ROOT/_ref/orig-bin/$file" "$RUN/full/$side/game/$file"
        done
        {
            cat "$ROOT/tests/c/m5_input.txt"
            cat "$ROOT/tests/c/saveload_keys.txt"
        } >"$RUN/full/$side/input.txt"
    done
    for side in lin win; do
        if [ "$side" = lin ]; then
            GAME="$BUILD/yt2game"
        else
            GAME="$WIN/yt2game.exe"
        fi
        set +e
        (
            cd "$RUN/full/$side"
            if [ "$side" = lin ]; then
                timeout 1200s "$GAME" --game game --link --ticks 9000 \
                    --no-sound --transfers 500000000 --pin-dos-time 0x1234 \
                    --input-script input.txt --present-log present.txt \
                    --dump dump >stdout.log 2>stderr.log
            else
                timeout 1200s wine "$GAME" --game game --link --ticks 9000 \
                    --no-sound --transfers 500000000 --pin-dos-time 0x1234 \
                    --input-script input.txt --present-log present.txt \
                    --dump dump >stdout.log 2>stderr.log
            fi
        )
        STATUS=$?
        set -e
        if [ "$STATUS" -ne 0 ]; then
            cat "$RUN/full/$side/stderr.log" >&2
            echo "windows: full $side run failed with status $STATUS" >&2
            exit 1
        fi
    done
    for file in SAVGAME1 SAVGAME2 SAVGAME3; do
        if ! cmp -s "$RUN/full/lin/game/$file" "$RUN/full/win/game/$file"; then
            echo "windows: $file differs between Linux and Windows" >&2
            exit 1
        fi
    done
    for file in stdout.log stderr.log present.txt; do
        if ! cmp -s "$RUN/full/lin/$file" "$RUN/full/win/$file"; then
            echo "windows: full $file differs between Linux and Windows" >&2
            exit 1
        fi
    done
    if ! diff -r "$RUN/full/lin/dump" "$RUN/full/win/dump" \
            >"$RUN/full/dump.diff" 2>&1; then
        cat "$RUN/full/dump.diff" >&2
        echo "windows: the full checkpoint dump differs" >&2
        exit 1
    fi
    echo "windows: full M5 + save/load run matches"
fi

echo "windows: OK (artifacts in $RUN)"
