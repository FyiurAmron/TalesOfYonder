#!/bin/sh
# Check that a bad record in a live INPUT.BIN does not kill the display
# link.  The client recreates INPUT.BIN for every session and the engine's
# own OUTPUT.BIN wraps; a read offset taken from a live file can land inside
# a record.  The input transport must skip to the next plausible record and
# keep presenting instead of freezing the client on its last frame.
#
#   sh tests/c/link-resync.sh [--no-build]
#
# The engine opens a header+record INPUT.BIN at its end, then the file grows
# with ten junk bytes (an impossible record length) and a valid key record.
# With the resync the key still arrives and playback continues; the old
# behaviour was "link lost; continuing without the client" and a frozen
# picture.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/link-resync/$STAMP"
GAME="$BUILD/yt2game"
TICKS=800
DO_BUILD=auto

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "link-resync: unknown option $1" >&2
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
        echo "link-resync: build failed" >&2
        exit 1
    fi
fi
if [ ! -x "$GAME" ]; then
    echo "link-resync: missing $GAME" >&2
    exit 1
fi

for file in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$file" "$RUN/game/$file"
done

python3 "$ROOT/tests/c/check.py" write-input "$RUN/game/INPUT.BIN" key \
    >"$RUN/input.log" 2>&1

"$GAME" --game "$RUN/game" --link --ticks "$TICKS" --trace-input \
    >"$RUN/stdout.log" 2>"$RUN/stderr.log" &
ENGINE=$!
trap 'kill "$ENGINE" 2>/dev/null || true' EXIT INT TERM
sleep 0.4

python3 - "$RUN/game/INPUT.BIN" <<'PY'
import struct
import sys

path = sys.argv[1]
with open(path, "ab") as target:
    target.write(b"\xEE" * 10)
    payload = bytes([0, 1, 0x1E, 0x61])
    frame = struct.pack("<HBBBBII", 0x5954, 0, 3, 0, 0, 1, len(payload)) + payload
    target.write(struct.pack("<II", 9000, len(frame)))
    target.write(frame)
PY

wait "$ENGINE" 2>/dev/null || true
trap - EXIT INT TERM

if grep -q "link lost" "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "link-resync: the engine dropped the display link" >&2
    exit 1
fi
if ! grep -q "input key scan=1E ascii=61" "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "link-resync: the record after the junk bytes never arrived" >&2
    exit 1
fi
if ! grep -q "input resynchronised after 10 byte(s)" "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "link-resync: the input transport did not resynchronise" >&2
    exit 1
fi
if ! grep -q "exited=0" "$RUN/stderr.log"; then
    cat "$RUN/stderr.log" >&2
    echo "link-resync: the game did not finish" >&2
    exit 1
fi

python3 - "$RUN/game/OUTPUT.BIN" <<'PY'
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
header = struct.Struct("<HBBBBII")
event = struct.Struct("<II")
offset = 16
frames = 0
while offset + event.size <= len(data):
    _stamp, size = event.unpack_from(data, offset)
    if offset + event.size + size > len(data) or size < header.size:
        break
    _magic, _version, family, _flags, _codec, _seq, length = header.unpack_from(
        data, offset + event.size)
    if family == 1 and length >= 1:
        payload = data[offset + event.size + header.size:
                       offset + event.size + header.size + length]
        if payload and payload[0] == 3:
            frames += 1
    offset += event.size + size
if frames < 50:
    raise SystemExit(f"link-resync: only {frames} frames were presented")
print(f"link-resync: OK ({frames} frames after the resync)")
PY

echo "link-resync: artifacts in $RUN"

# The client follows OUTPUT.BIN at the live edge.  If the engine was in the
# middle of a record when the follower joined, the first "record header" it
# reads is garbage; it must scan to the next record instead of killing the
# reader thread (which used to leave the window frozen on the last frame).
FOLLOW="$ROOT/build/runs/link-resync/$STAMP/follow"
CLIENT="$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll"
if [ -f "$CLIENT" ]; then
    mkdir -p "$FOLLOW"
    python3 - "$FOLLOW/OUTPUT.BIN" <<'PY'
import struct
import sys

header = struct.Struct("<4sHHIB3x")
frame_header = struct.Struct("<HBBBBII")


def frame(family, sequence, payload):
    return frame_header.pack(0x5954, 0, family, 0, 0, sequence,
                             len(payload)) + payload


with open(sys.argv[1], "wb") as target:
    target.write(header.pack(b"YT2B", 1, header.size, 1_193_182, 0))
PY
    dotnet "$CLIENT" --follow "$FOLLOW/OUTPUT.BIN" --headless --no-audio \
        --stats 1 >"$FOLLOW/client.log" 2>&1 &
    CLIENT_PID=$!
    trap 'kill "$CLIENT_PID" 2>/dev/null || true' EXIT INT TERM
    sleep 0.5
    python3 - "$FOLLOW/OUTPUT.BIN" <<'PY'
import struct
import sys

frame_header = struct.Struct("<HBBBBII")


def frame(family, sequence, payload):
    return frame_header.pack(0x5954, 0, family, 0, 0, sequence,
                             len(payload)) + payload


with open(sys.argv[1], "ab") as target:
    target.write(b"\xEE" * 10)
    records = [
        (1000, frame(1, 0, bytes([0, 0x40, 0x01, 0xC8, 0x00, 8]))),
        (2000, frame(1, 1, bytes([3]))),
    ]
    for stamp, raw in records:
        target.write(struct.pack("<II", stamp, len(raw)))
        target.write(raw)
PY
    sleep 1.5
    kill "$CLIENT_PID" 2>/dev/null || true
    wait "$CLIENT_PID" 2>/dev/null || true
    trap - EXIT INT TERM
    if ! grep -q "resynchronised" "$FOLLOW/client.log"; then
        cat "$FOLLOW/client.log" >&2
        echo "link-resync: the follower did not resynchronise" >&2
        exit 1
    fi
    if ! grep -q "frames=1 .*mode=320x200x8" "$FOLLOW/client.log"; then
        cat "$FOLLOW/client.log" >&2
        echo "link-resync: the follower lost the recovered frame" >&2
        exit 1
    fi
    echo "link-resync: follow resync OK"
fi
