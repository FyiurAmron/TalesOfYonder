#!/bin/sh
# Assemble the STREAM-instrumented variant (M1b boundary spike).
# Not byte-identical on purpose; the oracle build stays build/uasm/.
# Output: build/stream/ (exe + the minimum game files).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$ASM_DIR/.." && pwd)"
OUT="$ROOT/build/stream"
REFBIN="$ROOT/_ref/orig-bin"
mkdir -p "$OUT"
uasm -nologo -mz -DSTREAM -Fo="$OUT/SWREG.EXE" "$ASM_DIR/src/swreg.asm"
python3 "$SCRIPT_DIR/fixhdr.py" "$OUT/SWREG.EXE"
python3 - "$OUT/SWREG.EXE" <<'PY'
# The game uses hard-coded DS offsets, so S1DEA must keep its original paragraph
# prefix (7); see docs/stream.md. Fail loudly if a code change moves it.
import sys
d = open(sys.argv[1], "rb").read()
start = d.find(bytes.fromhex("00CE6EAE00D66EC400B66E8200BE6E98"))
name = d.find(b"EMMXXXX0")
if start < 0 or name < 0 or start % 16 != 7 or name - start != 0xC99:
    mod = "?" if start < 0 else str(start % 16)
    delta = "?" if start < 0 or name < 0 else hex(name - start)
    sys.exit("build-stream: S1DEA alignment drifted (prefix %s, EMM delta %s); "
             "adjust the pad at the end of S1DC9" % (mod, delta))
print("stream build: S1DEA prefix 7, EMMXXXX0 delta 0xC99")
PY
for f in CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp -f "$REFBIN/$f" "$OUT/$f"
done
