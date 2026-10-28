#!/bin/sh
# Build the C core, the stub, the transliterated game and the unit tests.
#
# The game sources are emitted from the annotated UASM source into the build
# tree (never committed): UASM produces the listing, tools/port/emit_c.py
# transliterates it against the original EXE image and the Makefile compiles
# the generated C with the hand-written machine boundary.
#
#   sh engine/c/tools/build.sh [make target ...]
#
# Set BUILD to override build/c.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
BUILD="${BUILD:-$ROOT/build/c}"
GENERATED="$BUILD/generated"
PORT="$BUILD/port"
ASM="$ROOT/engine/asm/swreg/src/swreg.asm"
EXE="$ROOT/_ref/orig-bin/SWREG.EXE"

mkdir -p "$GENERATED" "$PORT"

uasm -nologo -mz -Fl="$PORT/swreg.lst" -Fo="$PORT/swreg.exe" "$ASM"
python3 "$ROOT/tools/port/emit_c.py" \
    --exe "$EXE" --listing "$PORT/swreg.lst" --out "$GENERATED"

make -C "$ROOT/engine/c" BUILD="$BUILD" GENERATED="$GENERATED" "$@"
echo "build: c -> $BUILD"
