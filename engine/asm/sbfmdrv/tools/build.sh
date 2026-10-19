#!/bin/sh
# Assemble engine/asm/sbfmdrv/src/sbfmdrv.asm (UASM, raw COM) into build/sbfmdrv/.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
OUT="$ROOT/build/sbfmdrv"
mkdir -p "$OUT"
uasm -nologo -bin -Fo="$OUT/SBFMDRV.COM" -Fl="$OUT/SBFMDRV.lst" "$ROOT/engine/asm/sbfmdrv/src/sbfmdrv.asm"
