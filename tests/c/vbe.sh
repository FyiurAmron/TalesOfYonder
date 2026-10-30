#!/bin/sh
# Check the VBE publisher logo on the live folder link.  The game switches to
# VBE 101h (640x480x8), decodes LOGO.PCX into the banked aperture and loads
# the palette last.  The client must get set_mode, a blank frame, and then
# the complete logo before the game returns to mode 13h.
#
#   sh tests/c/vbe.sh              # build if needed, run 320 ticks
#   sh tests/c/vbe.sh --no-build   # use build/c/yt2game as it is
#   sh tests/c/vbe.sh --build      # force a rebuild
#
# The expected hash is the original's final logo frame from the oracle stream
# recording (build/runs/stream/.../frame_0010.png), which the C frame matches
# pixel for pixel.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/vbe/$STAMP"
GAME="$BUILD/yt2game"
TICKS=320
EXPECTED=6a51cf21adb30266
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "vbe: unknown option $1" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN/game"
if [ "$DO_BUILD" = yes ] || { [ "$DO_BUILD" = auto ] && [ ! -x "$GAME" ]; } ||
   [ -n "$(find "$ROOT/engine/c/src" "$ROOT/engine/c/include" \
                 "$ROOT/engine/c/Makefile" "$ROOT/engine/c/tools/build.sh" \
                 -newer "$GAME" -print -quit 2>/dev/null)" ]; then
    if ! sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1; then
        cat "$RUN/build.log" >&2
        echo "vbe: build failed" >&2
        exit 1
    fi
fi

for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$file" "$RUN/game/$file"
done
python3 "$ROOT/tests/c/check.py" write-input "$RUN/game/INPUT.BIN" \
    full --header-only >/dev/null

timeout 120s "$GAME" --game "$RUN/game" --link --ticks "$TICKS" \
    >"$RUN/engine.log" 2>&1
if ! grep -q 'exited=0' "$RUN/engine.log"; then
    cat "$RUN/engine.log" >&2
    echo "vbe: the game exited early" >&2
    exit 1
fi

python3 "$ROOT/tools/stream/replay.py" "$RUN/game/OUTPUT.BIN" \
    --out "$RUN/frames" --quiet

python3 - "$RUN/frames" "$EXPECTED" <<'PY'
import sys
from pathlib import Path
from PIL import Image

frames = Path(sys.argv[1])
expected = int(sys.argv[2], 16)
vbe = []
for path in sorted(frames.glob("*.png")):
    image = Image.open(path).convert("RGB")
    if image.size == (640, 480):
        vbe.append(image)
if len(vbe) < 2:
    raise SystemExit(f"vbe: only {len(vbe)} 640x480 frames; want blank + logo")

def nonblack(image):
    data = image.tobytes()
    return sum(1 for index in range(0, len(data), 3)
               if data[index:index + 3] != b"\0\0\0")

blank = sum(1 for image in vbe if nonblack(image) == 0)
if blank < 1:
    raise SystemExit("vbe: no blank frame after set_mode(640,480)")

logo = vbe[-1]
data = logo.tobytes()
hash_value = 0xCBF29CE484222325
for byte in data:
    hash_value ^= byte
    hash_value = (hash_value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
if hash_value != expected:
    raise SystemExit(
        f"vbe: logo hash {hash_value:016x} does not match {expected:016x}")
pixels = nonblack(logo)
if pixels != 17776:
    raise SystemExit(f"vbe: logo has {pixels} non-black pixels; want 17776")
print(f"vbe: OK ({len(vbe)} 640x480 frames, blank {blank}, "
      f"logo hash {hash_value:016x}, {pixels} non-black pixels)")
PY

# Keep the log but not the replay frames.
rm -rf "$RUN/frames"
echo "vbe: artifacts in $RUN"
