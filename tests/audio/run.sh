#!/bin/sh
# Offline gate for the client's OPL2 synthesis and SDL audio output.
#
#   sh tests/audio/run.sh
#
# Two independent checks:
#
#   1. `--dro` renders tests/data/music-walk.dro (a DOSBox-X raw OPL capture
#      of a real WORLD.DAT music block played through the resident SBFMDRV)
#      with the same Opl2 core the live client uses.  The WAV must be audible
#      (peak/RMS/duration) and the notes must land at the right pitches.
#   2. The live client path opens SDL audio and pushes frames; the dummy
#      drivers make that headless.  This catches the C# binding and the
#      stream plumbing, which the DRO check cannot.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$ROOT/build/runs/audio/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

dotnet build "$ROOT/client" -c Release >/dev/null
CLIENT=$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll

echo "audio: synthesising tests/data/music-walk.dro"
dotnet "$CLIENT" --dro "$ROOT/tests/data/music-walk.dro" --wav "$WORK/music-walk.wav"
python3 "$ROOT/tools/stream/wav.py" "$WORK/music-walk.wav"

python3 - "$WORK/music-walk.wav" <<'PY'
import array
import math
import sys
import wave

path = sys.argv[1]
w = wave.open(path, "rb")
rate = w.getframerate()
channels = w.getnchannels()
frames = w.getnframes()
duration = frames / rate
samples = array.array("h")
samples.frombytes(w.readframes(frames))
if channels > 1:
    samples = samples[::channels]

if not (12.0 <= duration <= 13.0):
    raise SystemExit(f"audio: duration {duration:.2f}s is outside 12..13s")
if len(samples) != frames:
    raise SystemExit("audio: channel layout is not the expected s16 stereo")

peak = max(abs(v) for v in samples)
rms = math.sqrt(sum(v * v for v in samples) / len(samples))
if not (0.30 * 32767 <= peak <= 0.55 * 32767):
    raise SystemExit(f"audio: peak {peak / 327.67:.1f}% FS is outside 30..55%")
if not (1800.0 <= rms <= 2800.0):
    raise SystemExit(f"audio: rms {rms:.0f} is outside 1800..2800")

# Goertzel magnitude of one frequency in a short window.
def magnitude(t, freq, window=0.15):
    start = int(t * rate)
    count = int(window * rate)
    data = samples[start:start + count]
    w = 2.0 * math.pi * freq / rate
    coeff = 2.0 * math.cos(w)
    s0 = s1 = s2 = 0.0
    for v in data:
        s0 = v + coeff * s1 - s2
        s2 = s1
        s1 = s0
    power = s1 * s1 + s2 * s2 - coeff * s1 * s2
    return math.sqrt(max(0.0, power)) / max(1, len(data))

# Pitches taken from the DRO's first bars: a low note, its octave, the
# third-harmonic-rich chord, and the later figure.  The DRO is deterministic,
# so these do not depend on timing or the audio device.
checks = [
    (0.2, 130.0, (196.0, 260.0, 390.0)),
    (0.5, 260.0, (130.0, 196.0, 390.0)),
    (1.0, 390.0, (260.0, 780.0)),
    (2.0, 260.0, (130.0, 520.0)),
    (3.0, 260.0, (1300.0, 1553.0)),
    (7.0, 130.0, (196.0, 260.0)),
    (9.0, 260.0, (196.0, 2333.0)),
]
for t, want, others in checks:
    level = magnitude(t, want)
    for other in others:
        rival = magnitude(t, other)
        if level <= rival:
            raise SystemExit(
                f"audio: at {t:.1f}s {want:.0f}Hz ({level:.0f}) is not above "
                f"{other:.0f}Hz ({rival:.0f})")
print("audio: pitch checks OK")
PY

# 3. The same DRO through the protocol path: encode its writes as
#    opl_batch_timed (the stamps are the DRO's milliseconds in 8253 counts),
#    replay the recording with --file --wav, and require a byte-identical WAV.
#    This pins the wire layout, the client's stamp scheduler and the synth to
#    the direct DRO render.
echo "audio: rendering the DRO through the timed protocol path"
python3 "$ROOT/tests/replay/mkdrostream.py" "$ROOT/tests/data/music-walk.dro" \
    "$WORK/music-walk-timed.bin"
dotnet "$CLIENT" --file "$WORK/music-walk-timed.bin" --wav "$WORK/music-walk-timed.wav"
if cmp -s "$WORK/music-walk.wav" "$WORK/music-walk-timed.wav"; then
    echo "audio: timed replay is sample-identical to the DRO render"
else
    echo "audio: FAIL - the timed replay differs from the DRO render" >&2
    exit 1
fi

echo "audio: opening the client's SDL stream (dummy drivers)"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    dotnet "$CLIENT" --file "$ROOT/tests/data/sample-audio.bin" --no-keyframe --stats 0 \
    >"$WORK/client.log" 2>&1
cat "$WORK/client.log"
grep -q 'client: audio 44100 Hz stereo s16' "$WORK/client.log"
grep -Eq 'client: audio [1-9][0-9]* frames, [0-9]+ underruns' "$WORK/client.log"
grep -q 'audio=7w/2b/1c' "$WORK/client.log"

echo "audio: OK (artifacts in $WORK)"
