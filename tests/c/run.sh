#!/bin/sh
# M2 gate: build the C core, run its unit tests, replay the asm fixture through
# the folder backend byte-for-byte, exercise the stub's folder/stream inputs,
# and render its output with both the client and the Python decoder.
#
#   sh tests/c/run.sh [--no-build]
#
# Needs a C11 compiler and dotnet.  Artifacts land in build/runs/c/<stamp>/.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/c/$STAMP"
DO_BUILD=1

for argument in "$@"; do
    case "$argument" in
        --no-build) DO_BUILD=0 ;;
        *)
            echo "c: unknown option $argument" >&2
            exit 2
            ;;
    esac
done

mkdir -p "$RUN"
if [ "$DO_BUILD" = 1 ]; then
    sh "$ROOT/engine/c/tools/build.sh" >"$RUN/build.log" 2>&1 || {
        cat "$RUN/build.log"
        exit 1
    }
fi
if [ ! -x "$BUILD/yt2stub" ] || [ ! -x "$BUILD/yt2core-tests" ]; then
    echo "c: missing build output in $BUILD" >&2
    exit 1
fi

echo "c: unit tests"
"$BUILD/yt2core-tests" | tee "$RUN/unit.log"

echo "c: asm fixture parity"
mkdir -p "$RUN/replay"
"$BUILD/yt2stub" --transport folder \
    --output "$RUN/replay/OUTPUT.BIN" --input "$RUN/replay/INPUT.BIN" \
    --replay-v0 "$ROOT/tests/data/sample-video.bin" >"$RUN/replay/log" 2>&1
python3 "$ROOT/tests/c/check.py" replay "$RUN/replay/OUTPUT.BIN" \
    "$ROOT/tests/data/sample-video.bin"

echo "c: stub demo"
mkdir -p "$RUN/demo"
"$BUILD/yt2stub" --transport folder \
    --output "$RUN/demo/OUTPUT.BIN" --input "$RUN/demo/INPUT.BIN" \
    --frames 8 --no-wait >"$RUN/demo/log" 2>&1
python3 "$ROOT/tests/c/check.py" demo "$RUN/demo/OUTPUT.BIN" --frames 8

echo "c: python render"
python3 "$ROOT/tools/stream/replay.py" "$RUN/demo/OUTPUT.BIN" \
    --out "$RUN/demo/python-frames" --ppm "$RUN/demo/python.ppm" \
    >"$RUN/demo/python.log" 2>&1

echo "c: client render"
dotnet build "$ROOT/client" -c Release >"$RUN/demo/client-build.log" 2>&1
dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" \
    --file "$RUN/demo/OUTPUT.BIN" --headless --no-audio --stats 0 \
    --ppm "$RUN/demo/client.ppm" >"$RUN/demo/client.log" 2>&1
if ! cmp -s "$RUN/demo/python.ppm" "$RUN/demo/client.ppm"; then
    echo "c: client and Python renders differ" >&2
    exit 1
fi
if ! grep -q 'unknown=0' "$RUN/demo/client.log"; then
    echo "c: client reported unknown messages" >&2
    cat "$RUN/demo/client.log"
    exit 1
fi
echo "c: client render matches the Python decoder"

echo "c: keyframe input"
mkdir -p "$RUN/keyframe"
python3 "$ROOT/tests/c/check.py" write-input "$RUN/keyframe/INPUT.BIN" full >/dev/null
"$BUILD/yt2stub" --transport folder \
    --output "$RUN/keyframe/OUTPUT.BIN" --input "$RUN/keyframe/INPUT.BIN" \
    --input-start --frames 3 --no-wait --verbose >"$RUN/keyframe/log" 2>&1
python3 "$ROOT/tests/c/check.py" input "$RUN/keyframe/OUTPUT.BIN"
for text in "client hello role=1 version=0" "keyframe request" "key scan=0x1E ascii=0x61" "mouse move 5,-2"; do
    if ! grep -q "$text" "$RUN/keyframe/log"; then
        echo "c: keyframe run did not log '$text'" >&2
        cat "$RUN/keyframe/log"
        exit 1
    fi
done

echo "c: key delta"
mkdir -p "$RUN/key"
python3 "$ROOT/tests/c/check.py" write-input "$RUN/key/INPUT.BIN" key >/dev/null
"$BUILD/yt2stub" --transport folder \
    --output "$RUN/key/OUTPUT.BIN" --input "$RUN/key/INPUT.BIN" \
    --input-start --frames 3 --no-wait >"$RUN/key/log" 2>&1
python3 "$ROOT/tests/c/check.py" keydelta "$RUN/key/OUTPUT.BIN"

echo "c: quit input"
mkdir -p "$RUN/quit"
python3 "$ROOT/tests/c/check.py" write-input "$RUN/quit/INPUT.BIN" quit >/dev/null
timeout 10s "$BUILD/yt2stub" --transport folder \
    --output "$RUN/quit/OUTPUT.BIN" --input "$RUN/quit/INPUT.BIN" \
    --input-start --frames 0 --no-wait >"$RUN/quit/log" 2>&1
grep -q 'quit' "$RUN/quit/log"
grep -q 'done frames=1' "$RUN/quit/log"

echo "c: live folder input"
mkdir -p "$RUN/live"
python3 "$ROOT/tests/c/check.py" write-input "$RUN/live/INPUT.BIN" full --header-only >/dev/null
timeout 10s "$BUILD/yt2stub" --transport folder \
    --output "$RUN/live/OUTPUT.BIN" --input "$RUN/live/INPUT.BIN" \
    --frames 0 --fps 100 >"$RUN/live/log" 2>&1 &
LIVE_PID=$!
for _ in $(seq 1 300); do
    [ -s "$RUN/live/OUTPUT.BIN" ] && break
    sleep 0.01
done
if [ ! -s "$RUN/live/OUTPUT.BIN" ]; then
    echo "c: live stub did not create OUTPUT.BIN" >&2
    kill "$LIVE_PID" 2>/dev/null || true
    exit 1
fi
python3 "$ROOT/tests/c/check.py" write-input "$RUN/live/INPUT.BIN" full --append >/dev/null
for _ in $(seq 1 300); do
    grep -q 'key scan=0x1E' "$RUN/live/log" && break
    sleep 0.01
done
if ! grep -q 'key scan=0x1E' "$RUN/live/log"; then
    echo "c: live stub did not consume appended input" >&2
    cat "$RUN/live/log"
    kill "$LIVE_PID" 2>/dev/null || true
    exit 1
fi
# Give the keyframe present time to land before asking the stub to quit.
sleep 0.2
python3 "$ROOT/tests/c/check.py" write-input "$RUN/live/INPUT.BIN" quit --append >/dev/null
wait "$LIVE_PID"
grep -q 'quit' "$RUN/live/log"
python3 "$ROOT/tests/c/check.py" live "$RUN/live/OUTPUT.BIN"

echo "c: stream backend"
mkdir -p "$RUN/stream"
"$BUILD/yt2stub" --transport stream --output "$RUN/stream/OUTPUT.v0" \
    --frames 2 --no-wait >"$RUN/stream/log" 2>&1
python3 "$ROOT/tests/c/check.py" stream "$RUN/stream/OUTPUT.v0" --frames 2

echo "c: null backend"
"$BUILD/yt2stub" --transport null --frames 2 --no-wait >"$RUN/null.log" 2>&1
grep -q 'done frames=2' "$RUN/null.log"

echo "c: OK (artifacts in $RUN)"
