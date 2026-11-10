#!/bin/sh
# Cross-build the C engine for Windows x64 with MinGW-w64.
#
#   sh engine/c/tools/build-mingw.sh [make target ...]
#
#   CC=x86_64-w64-mingw32-gcc   cross compiler (default; on a native MSYS2
#                               MinGW64 shell it falls back to gcc when that
#                               targets mingw)
#   BUILD=...                   output (default build/c-win)
#   GENERATED=...               emitted game sources (default build/c/generated;
#                               run engine/c/tools/build.sh first if missing)
#
# The result is a set of PE32+ x86-64 programs (yt2game, yt2stub, the unit
# tests and yt2saveinfo).  tests/c/windows.sh builds and runs them under Wine.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
BUILD="${BUILD:-$ROOT/build/c-win}"
GENERATED="${GENERATED:-$ROOT/build/c/generated}"
if [ -z "${CC:-}" ]; then
    if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        CC=x86_64-w64-mingw32-gcc
    elif command -v gcc >/dev/null 2>&1 &&
         gcc -dumpmachine 2>/dev/null | grep -q mingw; then
        CC=gcc
    else
        CC=x86_64-w64-mingw32-gcc
    fi
fi

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "build-mingw: $CC is not on PATH" >&2
    echo "  install mingw-w64 (gcc-mingw-w64-x86-64) or set CC" >&2
    exit 1
fi
if [ ! -f "$GENERATED/game_dispatch.c" ]; then
    echo "build-mingw: $GENERATED/game_dispatch.c is missing" >&2
    echo "  run: sh engine/c/tools/build.sh" >&2
    exit 1
fi

mkdir -p "$BUILD"
make -C "$ROOT/engine/c" BUILD="$BUILD" GENERATED="$GENERATED" \
    CC="$CC" EXEEXT=.exe \
    CFLAGS="-std=c11 -O2 -g -D__USE_MINGW_ANSI_STDIO=1" "$@"

if [ "$#" -eq 0 ] && command -v file >/dev/null 2>&1; then
    for program in yt2game yt2stub yt2core-tests yt2game-tests yt2saveinfo; do
        if ! file "$BUILD/$program.exe" | grep -q 'PE32+.*x86-64'; then
            echo "build-mingw: $BUILD/$program.exe is not a PE32+ x86-64 image" >&2
            exit 1
        fi
    done
fi
echo "build-mingw: windows x64 -> $BUILD"
