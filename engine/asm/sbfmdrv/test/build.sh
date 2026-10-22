#!/bin/sh
# Assemble engine/asm/sbfmdrv/test/*.asm (raw COMs, UASM) into build/sbfmdrv/test/.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
OUT="$ROOT/build/sbfmdrv/test"
mkdir -p "$OUT"
uasm -nologo -bin -Fo="$OUT/TEST.COM" "$SCRIPT_DIR/test.asm"
uasm -nologo -bin -Fo="$OUT/MUSIC.COM" "$SCRIPT_DIR/music.asm"
uasm -nologo -bin -Fo="$OUT/LATE.COM" "$SCRIPT_DIR/late.asm"
