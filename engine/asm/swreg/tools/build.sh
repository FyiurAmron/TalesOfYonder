#!/bin/sh
# Assemble src/swreg.asm (UASM), patch the MZ header, and stage a runnable dir.
# Outputs go to <repo>/build/uasm/ — the exe plus the minimum game files from _ref/orig-bin.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$ASM_DIR/.." && pwd)"
OUT="$ROOT/build/uasm"
REFBIN="$ROOT/_ref/orig-bin"
mkdir -p "$OUT"
uasm -nologo -mz -Fo="$OUT/SWREG.EXE" "$ASM_DIR/src/swreg.asm"
python3 "$SCRIPT_DIR/fixhdr.py" "$OUT/SWREG.EXE"
( cd "$OUT" && md5sum -c "$SCRIPT_DIR/SWREG.EXE.md5" )
for f in CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp -f "$REFBIN/$f" "$OUT/$f"
done
