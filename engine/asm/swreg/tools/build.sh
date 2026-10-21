#!/bin/sh
# Assemble src/swreg.asm (UASM), patch the MZ header, and stage a runnable dir.
#
#   sh build.sh [--variant NAME]
#
#     oracle  (default)  the port: byte-identical to the reference; the gate build
#     serial   -DSEAM             protocol-v0 VIDEO messages over COM1 (client link)
#     file     + -DSEAM_FILE      write the messages to STREAM.BIN instead of COM1
#     tee      + -DSEAM_TEE       both at once; the file is the link's ground truth
#     log      + -DSEAM_LOG       file backend plus SEAMLOG.TXT debug records
#
# Output: <repo>/build/swreg/<variant>/ -- the exe plus the minimum game files
# from _ref/orig-bin, so the directory runs as staged.  The oracle build is
# checked against SWREG.EXE.md5 here; verify.sh runs the instruction-level gate.
# The seam variants are not byte-identical on purpose (docs/client.md).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$ASM_DIR/../../.." && pwd)"
REFBIN="$ROOT/_ref/orig-bin"
VARIANT=oracle

while [ $# -gt 0 ]; do
    case "$1" in
        --variant) VARIANT=$2; shift 2 ;;
        -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
        *) echo "build: unknown option $1" >&2; exit 2 ;;
    esac
done

case "$VARIANT" in
    oracle) DEFS= ;;
    serial) DEFS="-DSEAM" ;;
    file)   DEFS="-DSEAM -DSEAM_FILE" ;;
    tee)    DEFS="-DSEAM -DSEAM_FILE -DSEAM_TEE" ;;
    log)    DEFS="-DSEAM -DSEAM_FILE -DSEAM_LOG" ;;
    *) echo "build: unknown variant '$VARIANT' (oracle, serial, file, tee, log)" >&2
       exit 2 ;;
esac

OUT="$ROOT/build/swreg/$VARIANT"
mkdir -p "$OUT"
uasm -nologo -mz $DEFS -I"$ASM_DIR/src" -Fo="$OUT/SWREG.EXE" "$ASM_DIR/src/swreg.asm"
python3 "$SCRIPT_DIR/fixhdr.py" "$OUT/SWREG.EXE"
if [ "$VARIANT" = oracle ]; then
    ( cd "$OUT" && md5sum -c "$SCRIPT_DIR/SWREG.EXE.md5" )
else
    python3 "$SCRIPT_DIR/check_mz_layout.py" "$OUT/SWREG.EXE"
fi
for f in CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp -f "$REFBIN/$f" "$OUT/$f"
done
echo "build: $VARIANT -> $OUT"
