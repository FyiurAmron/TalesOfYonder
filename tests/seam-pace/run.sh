#!/bin/sh
# Check the seam's presentation budget (three passes per BIOS tick).
#
#   sh tests/seam-pace/run.sh
#
# The gate decides how often the game's screen goes to the client.  It is
# 16-bit assembly inside the seam, so it is tested the way the seam is: the
# routine is extracted verbatim from seam.asm, assembled into a .COM with a
# harness, run under DOSBox-X and checked here.  The harness drives it against
# the real BIOS tick - five calls inside one tick, a forced call on an
# exhausted budget, the first call of the next tick - and needs uasm,
# dosbox-x and Xvfb; those parts skip (exit 0) without them.
#
# Why the budget and not a finer clock: the BIOS tick is 55 ms and a pass per
# tick limited the client to 18.2 pictures a second, but SBFMDRV reprograms PIT
# channel 0 to about 64 Hz, so its counter reloads every 15.6 ms and cannot
# time a pass interval; see docs/sbfmdrv.md and the progress log.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$ROOT/build/runs/seam-pace/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

for tool in uasm dosbox-x Xvfb; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "seam-pace: SKIP - $tool is not installed" >&2
        exit 0
    fi
done

python3 "$ROOT/tests/seam-pace/extract.py" "$ROOT/engine/asm/swreg/src/seam.asm" \
    "$WORK/pace.inc"
cp "$ROOT/tests/seam-pace/pacetest.asm" "$WORK/pacetest.asm"
( cd "$WORK" && uasm -nologo -bin -Fo "pacetest.com" "pacetest.asm" ) >"$WORK/uasm.log" 2>&1 || {
    echo "seam-pace: FAIL - the harness does not assemble" >&2
    cat "$WORK/uasm.log" >&2
    exit 1
}

N=90
while [ "$N" -lt 120 ] && timeout 2 xwininfo -root -display ":$N" >/dev/null 2>&1; do
    N=$((N + 1))
done
if [ "$N" -ge 120 ]; then
    echo "seam-pace: FAIL - no free X display in :90..:119" >&2
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
pacetest.com
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
        echo "seam-pace: FAIL - Xvfb did not come up on $DISP" >&2
        cat "$WORK/xvfb.log" >&2
        exit 1
    fi
    sleep 0.1
done

rm -f "$WORK/PACE.BIN"
DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dosbox-x -conf "$WORK/dosbox.conf" >"$WORK/dosbox.log" 2>&1 &
DPID=$!
# The harness finishes in well under a tick; a hung one sits out the timeout.
I=0
while [ "$I" -lt 100 ] && ! [ -s "$WORK/PACE.BIN" ]; do
    sleep 0.1
    I=$((I + 1))
done
sleep 1
kill "$DPID" 2>/dev/null || true
DPID=

if ! [ -s "$WORK/PACE.BIN" ]; then
    echo "seam-pace: FAIL - the harness wrote nothing (hang or crash?)" >&2
    tail -5 "$WORK/dosbox.log" >&2 || true
    exit 1
fi

python3 "$ROOT/tests/seam-pace/check.py" "$WORK"
echo "seam-pace: artifacts in $WORK"
