#!/bin/sh
# Offline gate for the client's OPL2 synthesis and SDL audio output.
#
#   sh tests/audio/run.sh
#
# Four independent checks:
#
#   1. `--dro` renders tests/data/music-walk.dro (a DOSBox-X raw OPL capture
#      of a real WORLD.DAT music block played through the resident SBFMDRV)
#      with the same Opl2 core the live client uses.  The WAV must be audible
#      (peak/RMS/duration) and the notes must land at the right pitches.
#   2. The PC-speaker gate in sample-audio.bin must render as a square wave
#      only between its on/off edges, at the right level and pitch.
#   3. The WORLD.DAT VOC effects must decode and mix, and effect 3's sample
#      must replace its PC-speaker fallback pair.
#   4. The live client path opens SDL audio and pushes frames; the dummy
#      drivers make that headless.  This catches the C# binding and the
#      stream plumbing, which the offline checks cannot.
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

echo "audio: rendering the PC-speaker beep offline"
dotnet "$CLIENT" --file "$ROOT/tests/data/sample-audio.bin" --wav "$WORK/beep.wav"
python3 - "$WORK/beep.wav" <<'PY'
import array
import math
import sys
import wave

w = wave.open(sys.argv[1], "rb")
rate = w.getframerate()
frames = w.getnframes()
samples = array.array("h")
samples.frombytes(w.readframes(frames))
mono = samples[::w.getnchannels()]
duration = frames / rate
if not (1.4 <= duration <= 1.55):
    raise SystemExit(f"audio: beep render is {duration:.2f}s, expected ~1.47s")

def rms(t0, t1):
    part = mono[int(t0 * rate):int(t1 * rate)]
    return math.sqrt(sum(v * v for v in part) / max(1, len(part)))

beep = rms(0.28, 0.42)
if not (0.25 * 32767 <= beep <= 0.35 * 32767):
    raise SystemExit(f"audio: beep rms {beep:.0f} is outside 25..35% fs")
if rms(0.05, 0.15) != 0.0 or rms(0.55, 1.2) != 0.0:
    raise SystemExit("audio: the beep leaks outside its on/off window")
section = mono[int(0.28 * rate):int(0.42 * rate)]
crossings = sum(1 for a, b in zip(section, section[1:]) if (a < 0) != (b < 0))
hz = crossings / 0.14 / 2
if not (1700.0 <= hz <= 1900.0):
    raise SystemExit(f"audio: beep is {hz:.0f} Hz, expected ~1800 Hz")
print(f"audio: beep OK ({beep:.0f} rms, {hz:.0f} Hz)")
PY

echo "audio: rendering WORLD.DAT VOC effects and the effect-3 beep replacement"
python3 - "$WORK" <<'PY'
import struct
import sys

work = sys.argv[1]

def record(family, payload, seq):
    return struct.pack("<HBBBBII", 0x5954, 0, family, 0, 0, seq, len(payload)) + payload

def audio_control():
    return record(2, struct.pack("<BHHB", 2, 44100, 0, 1), 0)

def speaker(gate, divisor, stamp):
    return record(2, struct.pack("<BBHI", 4, gate, divisor, stamp), 0)

def sample(effect, offset, stamp):
    return record(2, struct.pack("<BHII", 5, effect, offset, stamp), 0)

# Effect 3 is the 0.20 s VOC at 0x09FFE6; effect 5 is the 1 s / 345 Hz
# record at 0x0A076A.  The speaker pair is the fallback beep that must
# disappear when the effect-3 sample is available.
sfx3 = b"".join([
    audio_control(),
    sample(3, 0x09FFE6, 0),
    speaker(1, 662, 0),
    speaker(0, 0, 262500),          # 220 ms at 1,193,182 Hz
])
open(f"{work}/sfx3.bin", "wb").write(sfx3)

sample3 = b"".join([
    audio_control(),
    sample(3, 0x09FFE6, 0),
])
open(f"{work}/sample3.bin", "wb").write(sample3)

sample5 = b"".join([
    audio_control(),
    sample(5, 0x0A076A, 0),
])
open(f"{work}/sample5.bin", "wb").write(sample5)

beep = b"".join([
    audio_control(),
    speaker(1, 662, 0),
    speaker(0, 0, 262500),
])
open(f"{work}/beep-only.bin", "wb").write(beep)
PY

WORLD="$ROOT/_ref/orig-bin/WORLD.DAT"
dotnet "$CLIENT" --file "$WORK/sfx3.bin" --wav "$WORK/sfx3.wav" --world "$WORLD"
dotnet "$CLIENT" --file "$WORK/sample3.bin" --wav "$WORK/sample3.wav" --world "$WORLD"
dotnet "$CLIENT" --file "$WORK/sample5.bin" --wav "$WORK/sample5.wav" --world "$WORLD"
dotnet "$CLIENT" --file "$WORK/beep-only.bin" --wav "$WORK/beep-only.wav"

python3 - "$WORK/sfx3.wav" "$WORK/sample3.wav" "$WORK/sample5.wav" \
    "$WORK/beep-only.wav" <<'PY'
import array
import math
import sys
import wave

def load(path):
    w = wave.open(path, "rb")
    rate = w.getframerate()
    frames = w.getnframes()
    samples = array.array("h")
    samples.frombytes(w.readframes(frames))
    return rate, frames, samples[::2]

def rms(data, t0, t1, rate):
    part = data[int(t0 * rate):int(t1 * rate)]
    return math.sqrt(sum(v * v for v in part) / max(1, len(part)))

def magnitude(data, freq, t0, t1, rate):
    part = data[int(t0 * rate):int(t1 * rate)]
    w = 2.0 * math.pi * freq / rate
    coeff = 2.0 * math.cos(w)
    s0 = s1 = s2 = 0.0
    for v in part:
        s0 = v + coeff * s1 - s2
        s2 = s1
        s1 = s0
    power = s1 * s1 + s2 * s2 - coeff * s1 * s2
    return math.sqrt(max(0.0, power)) / max(1, len(part))

sfx_rate, sfx_frames, sfx = load(sys.argv[1])
s3_rate, s3_frames, s3 = load(sys.argv[2])
s5_rate, s5_frames, s5 = load(sys.argv[3])
beep_rate, beep_frames, beep = load(sys.argv[4])

# Effect 3: a 0.20 s record plus the client's one-second tail.
if not (1.1 <= s3_frames / s3_rate <= 1.35):
    raise SystemExit(f"audio: effect-3 render is {s3_frames / s3_rate:.2f}s, expected ~1.2s")
if rms(s3, 0.02, 0.19, s3_rate) < 800:
    raise SystemExit("audio: the effect-3 VOC is silent")
if rms(s3, 0.30, 1.10, s3_rate) != 0.0:
    raise SystemExit("audio: the effect-3 VOC leaks past its block")
# The beep pair must be dropped: the mixed stream is the sample-only stream
# over the sample-only length.
if len(s3) > len(sfx) or s3 != sfx[:len(s3)]:
    raise SystemExit("audio: the effect-3 VOC did not replace the beep")

# Effect 5: a 1 s record at 345 Hz plus the tail; 1800 Hz is the beep
# control.
if not (1.9 <= s5_frames / s5_rate <= 2.1):
    raise SystemExit(f"audio: effect-5 render is {s5_frames / s5_rate:.2f}s, expected ~2s")
if rms(s5, 0.1, 0.9, s5_rate) < 800:
    raise SystemExit("audio: the effect-5 VOC is silent")
if rms(s5, 1.1, 1.9, s5_rate) != 0.0:
    raise SystemExit("audio: the effect-5 VOC leaks past its one-second block")
if magnitude(s5, 345, 0.1, 0.5, s5_rate) <= magnitude(s5, 1800, 0.1, 0.5, s5_rate):
    raise SystemExit("audio: the 345 Hz VOC tone is missing")

if magnitude(beep, 1800, 0.0, 0.2, beep_rate) < 1000:
    raise SystemExit("audio: the plain beep is missing")
print(f"audio: VOC effects OK (effect 3 {s3_frames / s3_rate:.2f}s rms "
      f"{rms(s3, 0.02, 0.19, s3_rate):.0f}, effect 5 {s5_frames / s5_rate:.2f}s "
      f"rms {rms(s5, 0.1, 0.9, s5_rate):.0f}), effect-3 beep replaced")
PY

echo "audio: opening the client's SDL stream (dummy drivers)"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    dotnet "$CLIENT" --file "$ROOT/tests/data/sample-audio.bin" --no-keyframe --stats 0 \
    >"$WORK/client.log" 2>&1
cat "$WORK/client.log"
grep -q 'client: audio 44100 Hz stereo s16' "$WORK/client.log"
grep -Eq 'client: audio [1-9][0-9]* frames, [0-9]+ underruns' "$WORK/client.log"
grep -q 'audio=7w/2b/1c' "$WORK/client.log"

echo "audio: OK (artifacts in $WORK)"
