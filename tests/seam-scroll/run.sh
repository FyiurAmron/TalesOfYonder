#!/bin/sh
# Check the seam's whole-screen scroll detector (VIDEO message 5).
#
#   sh tests/seam-scroll/run.sh
#
# The detector is 16-bit assembly inside the seam, so it is tested the way the
# seam is: the routines are extracted verbatim from seam.asm, assembled into a
# .COM with a harness, run under DOSBox-X, and checked here against an
# independent model of docs/protocol.md's rules.  Ten cases: pans of -1, -3,
# -2, +2, -5 and +8 rows (one with a row that changes on its own), a pan past
# the +-8 range, a repaint, and two still pictures whose changed rows must not
# be mistaken for a shift.  The harness half needs uasm, dosbox-x and Xvfb and
# skips (exit 0) without them; the format half always runs.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$ROOT/build/runs/seam-scroll/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

# The format half needs nothing but python3: the same pan as rows and as
# scroll messages must decode to the same pictures, and the committed fixtures
# must be what the generator produces.
python3 "$ROOT/tests/seam-scroll/mkstream.py" "$WORK/data"
for f in sample-pan-rows.bin sample-pan-scroll.bin; do
    if ! cmp -s "$WORK/data/$f" "$ROOT/tests/data/$f"; then
        echo "seam-scroll: FAIL - tests/data/$f is not what mkstream.py builds" >&2
        exit 1
    fi
done
python3 "$ROOT/tests/seam-scroll/check_stream.py" "$ROOT/tests/data"

for tool in uasm dosbox-x Xvfb; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "seam-scroll: SKIP - $tool is not installed" >&2
        exit 0
    fi
done

python3 "$ROOT/tests/seam-scroll/extract.py" "$ROOT/engine/asm/swreg/src/seam.asm" \
    "$WORK/scroll.inc"
cp "$ROOT/tests/seam-scroll/scrtest.asm" "$WORK/scrtest.asm"
( cd "$WORK" && uasm -nologo -bin -Fo "scrtest.com" "scrtest.asm" ) >"$WORK/uasm.log" 2>&1 || {
    echo "seam-scroll: FAIL - the harness does not assemble" >&2
    cat "$WORK/uasm.log" >&2
    exit 1
}

N=90
while [ "$N" -lt 120 ] && timeout 2 xwininfo -root -display ":$N" >/dev/null 2>&1; do
    N=$((N + 1))
done
if [ "$N" -ge 120 ]; then
    echo "seam-scroll: FAIL - no free X display in :90..:119" >&2
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
scrtest.com
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
        echo "seam-scroll: FAIL - Xvfb did not come up on $DISP" >&2
        cat "$WORK/xvfb.log" >&2
        exit 1
    fi
    sleep 0.1
done

rm -f "$WORK/SCR.BIN"
DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dosbox-x -conf "$WORK/dosbox.conf" >"$WORK/dosbox.log" 2>&1 &
DPID=$!
# A hung detector would sit out the timeout; a working one finishes in a second.
I=0
while [ "$I" -lt 100 ] && ! [ -s "$WORK/SCR.BIN" ]; do
    sleep 0.2
    I=$((I + 1))
done
sleep 1
kill "$DPID" 2>/dev/null || true
DPID=

if ! [ -s "$WORK/SCR.BIN" ]; then
    echo "seam-scroll: FAIL - the harness wrote nothing (hang or crash?)" >&2
    tail -5 "$WORK/dosbox.log" >&2 || true
    exit 1
fi

python3 "$ROOT/tests/seam-scroll/check.py" "$WORK"
echo "seam-scroll: artifacts in $WORK"
