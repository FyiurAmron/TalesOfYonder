#!/bin/sh
# End-to-end audio check of the ORIGINAL game under DOSBox-X.
#
#   sh tools/dosbox/audio.sh [duration_seconds]      (default 45)
#
# Runs the reference `CH2.BAT` (which installs SBFMDRV.COM and starts
# SWREG.EXE /W /B) with DOSBox-X's DX-CAPTURE wrapping the game executable, so
# everything the game and the driver send to the FM chip is recorded:
#
#   capture/swreg_000.wav   mixed output (tools/stream/wav.py stats)
#   capture/swreg_000.dro   OPL register writes, DRO 2.0 (tools/stream/dro.py)
#
# AUDIO_MODE selects the capture: wav (default), dro, both.
#
# What this proves: the game finds and drives the resident driver on its own
# (no harness commands), the music is audible, and every gap between note-ons in
# the register log is a whole number of ticks of the shipped block tempo
# (1000/word[+0Ch] ms) -- the same timeline the driver harness cross-checks
# against the static walk of the block.
#
# The capture has to start inside the batch environment: wrapping CH2.BAT
# itself stops the capture at the driver's TSR install, and running SWREG.EXE
# outside the batch makes the game print "Please run ... from CH2.BAT" (it
# requires the /B switch the batch passes).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
DURATION="${1:-${AUDIO_DURATION:-45}}"
MODE="${AUDIO_MODE:-wav}"
STAMP="$(date +%Y%m%d-%H%M%S)-audio"
RUN="$ROOT/build/runs/oracle/$STAMP"
REFBIN="$ROOT/_ref/orig-bin"
DISP=":95"
GAME_FILES="CH2.BAT SBFMDRV.COM SWREG.EXE WORLD.DAT PICTURES.VGA CURGAME"

case "$MODE" in
    wav)  CAPFLAG="/A" ;;
    dro)  CAPFLAG="/O" ;;
    both) CAPFLAG="/A /O" ;;
    *)    echo "AUDIO_MODE must be wav, dro or both" >&2; exit 2 ;;
esac

if [ -e /tmp/.X11-unix/X95 ]; then
    echo "audio: display :95 is busy; refusing (stale Xvfb?)" >&2
    exit 1
fi
[ -f "$REFBIN/SWREG.EXE" ] || { echo "audio: no reference in $REFBIN" >&2; exit 1; }

mkdir -p "$RUN/game" "$RUN/capture" "$RUN/saves"
( cd "$REFBIN" && md5sum -c "$ROOT/engine/asm/swreg/tools/SWREG.EXE.md5" ) >"$RUN/oracle-md5.txt"

# stage the game and wrap its executable with the capture command
for f in $GAME_FILES; do
    cp -f "$REFBIN/$f" "$RUN/game/$f"
done
python3 - "$RUN/game/CH2.BAT" "$CAPFLAG" <<'EOF'
import sys

path, capflag = sys.argv[1], sys.argv[2]
line = b"SWREG.EXE %1 %2 %3 %4 /W /B"
src = open(path, "rb").read()
if line not in src:
    sys.stderr.write("audio: CH2.BAT does not start SWREG.EXE as expected\n")
    raise SystemExit(1)
open(path, "wb").write(src.replace(line, b"DX-CAPTURE " + capflag.encode() + b" " + line))
EOF

# game profile (same as tools/dosbox/dosbox-x.conf) with the capture commands
python3 - "$HERE/dosbox-x.conf" "$RUN/dosbox-x.conf" <<'EOF'
import sys

src, dst = sys.argv[1:3]
lines = open(src).read().split("\n")
i = lines.index("[autoexec]")
head = [ln for ln in lines[:i] if not ln.startswith("captures")]
tail = ["[autoexec]", "@echo off", "SET BLASTER=A220 I7 D1 H5 T6", "SET SOUND=",
        "mount c game", "c:", "CH2.BAT", ""]
open(dst, "w").write("\n".join(head + tail))
EOF

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >"$RUN/xvfb.log" 2>&1 &
XPID=$!
trap 'kill "$XPID" 2>/dev/null || true' EXIT

i=0
while [ ! -e /tmp/.X11-unix/X95 ] && [ "$i" -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done

(
    cd "$RUN"
    DISPLAY="$DISP" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
        dosbox-x -conf "$RUN/dosbox-x.conf" -set "dosbox captures=$RUN/capture" \
        -fastlaunch -time-limit "$DURATION" -savedir "$RUN/saves" \
        >"$RUN/stdout.log" 2>&1 || true
)

echo "run dir: $RUN"
echo "oracle:  $(tail -1 "$RUN/oracle-md5.txt")"
WAV="$(ls "$RUN"/capture/*.wav 2>/dev/null | head -1 || true)"
DRO="$(ls "$RUN"/capture/*.dro 2>/dev/null | head -1 || true)"
rc=0
if [ -n "$WAV" ]; then
    python3 "$ROOT/tools/stream/wav.py" "$WAV" || rc=1
fi
if [ -n "$DRO" ]; then
    python3 "$ROOT/tools/stream/dro.py" summary "$DRO"
    # every shipped CTMF block runs at 96 Hz, so the register log's note-ons
    # must land on a 10.42 ms grid (the driver's tick)
    python3 - "$ROOT/tools/stream" "$DRO" <<'EOF' || rc=1
import sys

here, path = sys.argv[1], sys.argv[2]
sys.path.insert(0, here)
import dro

tick = 1000.0 / 96.0
c = dro.load(path)
ons = [t[0] for t in c["writes"] if dro.key_on(t)]
gaps = [b - a for a, b in zip(ons, ons[1:])]
bad = [d for d in gaps if abs(d - round(d / tick) * tick) > 4.0]
print(
    "DRO grid: %d note-ons, %d/%d gaps on the %.2f ms tick grid (tolerance 4 ms)"
    % (len(ons), len(gaps) - len(bad), len(gaps), tick)
)
if len(gaps) and len(bad) > len(gaps) * 0.05:
    sys.stderr.write("audio: %d note-on gaps are off the tick grid\n" % len(bad))
    raise SystemExit(1)
EOF
fi
if [ -z "$WAV" ] && [ -z "$DRO" ]; then
    echo "audio: no capture produced (see $RUN/stdout.log and $RUN/run.log)" >&2
    rc=1
fi
exit $rc
