#!/bin/sh
# Repro for the DOSBox-X nullmodem 0xFF escape.
#
#   sh tools/repro/nullmodem-0xff/run.sh [--serial LINE] [--seconds N] [--port N]
#                                        [--display :N] [--expect clean|dirty]
#
# Runs PATTERN.COM (an incrementing byte counter on COM1 at 115200 8N1) under
# DOSBox-X on a private Xvfb display, bridges COM1 to TCP with the nullmodem
# and checks what arrives at the TCP end.  --serial is the [serial] serial1
# line; the interesting variants are
#
#   "nullmodem port:2352 multiplier:1"                 -> every 0xFF doubled
#   "nullmodem port:2352 multiplier:1 transparent:1"   -> raw bytes
#
# Exit code: 0 when the run produced a verdict (0 clean, 1 dirty), 1 if it
# disagrees with --expect, 2 when nothing could be measured.
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
PORT=2352
RUN_SECONDS=5
DISPLAY_NUM=:89
SERIAL=
EXPECT=

while [ $# -gt 0 ]; do
    case "$1" in
        --serial) SERIAL=$2; shift 2 ;;
        --seconds) RUN_SECONDS=$2; shift 2 ;;
        --port) PORT=$2; shift 2 ;;
        --display) DISPLAY_NUM=$2; shift 2 ;;
        --expect) EXPECT=$2; shift 2 ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "repro: unknown option $1" >&2; exit 2 ;;
    esac
done
SERIAL=${SERIAL:-nullmodem port:$PORT multiplier:1}

# a private display, so parallel runs do not collide
while [ -e "/tmp/.X11-unix/X${DISPLAY_NUM#:}" ]; do
    DISPLAY_NUM=":$(( ${DISPLAY_NUM#:} + 1 ))"
done

WORK=$ROOT/build/runs/repro/nullmodem-0xff/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

uasm -bin -Fo "$WORK/PATTERN.COM" "$ROOT/tools/repro/nullmodem-0xff/pattern.asm" >/dev/null
cat >"$WORK/dosbox-x.conf" <<EOF
[dosbox]
memsize = 16
[serial]
serial1 = $SERIAL
[autoexec]
mount c "$WORK"
c:
PATTERN.COM
EOF

Xvfb "$DISPLAY_NUM" -screen 0 1024x768x24 -nolisten tcp >"$WORK/xvfb.log" 2>&1 &
XVFB_PID=$!
trap 'kill $XVFB_PID 2>/dev/null || true' EXIT INT TERM
sleep 2

DISPLAY=$DISPLAY_NUM SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    dosbox-x -conf "$WORK/dosbox-x.conf" -fastlaunch \
    -time-limit "$((RUN_SECONDS + 15))" >"$WORK/dosbox.log" 2>&1 &
RUN_PID=$!

echo "repro: serial1 = $SERIAL"
rc=0
python3 "$ROOT/tools/repro/nullmodem-0xff/check_pattern.py" \
    --port "$PORT" --seconds "$RUN_SECONDS" ${EXPECT:+--expect "$EXPECT"} || rc=$?
wait $RUN_PID 2>/dev/null || true
echo "repro: artifacts in $WORK"
exit $rc
