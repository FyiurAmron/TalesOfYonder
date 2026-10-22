#!/bin/sh
# Round-trip check for the seam's run-length row encoder (VIDEO codec 1).
#
#   sh tests/seam-rle/run.sh
#
# The encoder is 16-bit assembly inside the seam, so it is tested the way the
# seam is: the routines are extracted verbatim from seam.asm, assembled into a
# .COM with a harness, run under DOSBox-X and decoded here in Python.  Six rows
# of 640 pixels cover one long run (counts above 255), all-distinct and
# checkerboard rows (literal chunks), noise (no runs), two long runs, and runs
# mixed with singles.  Needs uasm, dosbox-x and Xvfb; skips (exit 0) without
# them.  `tests/replay/run.sh` covers the decoder end of the same format.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$ROOT/build/runs/seam-rle/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

for tool in uasm dosbox-x Xvfb; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "seam-rle: SKIP - $tool is not installed" >&2
        exit 0
    fi
done

python3 "$ROOT/tests/seam-rle/extract.py" "$ROOT/engine/asm/swreg/src/seam.asm" \
    "$WORK/encoder.inc"
cp "$ROOT/tests/seam-rle/rltest.asm" "$WORK/rltest.asm"
( cd "$WORK" && uasm -nologo -mz -Fo "rltest.com" "rltest.asm" ) >"$WORK/uasm.log" 2>&1 || {
    echo "seam-rle: FAIL - the harness does not assemble" >&2
    cat "$WORK/uasm.log" >&2
    exit 1
}

N=90
while [ "$N" -lt 120 ] && timeout 2 xwininfo -root -display ":$N" >/dev/null 2>&1; do
    N=$((N + 1))
done
if [ "$N" -ge 120 ]; then
    echo "seam-rle: FAIL - no free X display in :90..:119" >&2
    exit 1
fi
DISP=":$N"

cat >"$WORK/dosbox.conf" <<EOF
[sdl]
fullscreen=false
output=surface
autolock=false
[dosbox]
memsize=16
[cpu]
cycles=fixed 30000
[autoexec]
mount c $WORK
c:
rltest.com
exit
EOF

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >"$WORK/xvfb.log" 2>&1 &
XPID=$!
DPID=
cleanup() {
    [ -n "$DPID" ] && kill "$DPID" 2>/dev/null || true
    kill "$XPID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

I=0
until timeout 2 xwininfo -root -display "$DISP" >/dev/null 2>&1; do
    I=$((I + 1))
    if [ "$I" -ge 100 ]; then
        echo "seam-rle: FAIL - Xvfb did not come up on $DISP" >&2
        cat "$WORK/xvfb.log" >&2
        exit 1
    fi
    sleep 0.1
done

rm -f "$WORK/RLE.BIN" "$WORK/ORG.BIN"
DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dosbox-x -conf "$WORK/dosbox.conf" >"$WORK/dosbox.log" 2>&1 &
DPID=$!
# A hung encoder would sit out the timeout; a working one finishes in a second.
I=0
while [ "$I" -lt 100 ] && ! [ -s "$WORK/RLE.BIN" ]; do
    sleep 0.2
    I=$((I + 1))
done
sleep 1
kill "$DPID" 2>/dev/null || true
DPID=

if ! [ -s "$WORK/RLE.BIN" ]; then
    echo "seam-rle: FAIL - the harness wrote nothing (hang or crash?)" >&2
    tail -5 "$WORK/dosbox.log" >&2 || true
    exit 1
fi

python3 "$ROOT/tests/seam-rle/check.py" "$WORK"
echo "seam-rle: OK (six 640-pixel rows round-trip; artifacts in $WORK)"
