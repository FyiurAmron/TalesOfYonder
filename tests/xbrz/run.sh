#!/bin/sh
# xBRZ gate: the client's xBRZ filter must produce exactly the pixels the
# reference implementation produces.
#
# The client replays tests/data/sample-video.bin with --filter xbrz, which
# scales the last frame by the window scale (3) and writes the filtered
# pixels.  The expected pixels are the reference implementation's output for
# the same frame (Zenju's advanced xBRZ), stored as raw little endian RGBA.
#
# Needs dotnet.  Artifacts: build/runs/xbrz/<stamp>/.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SAMPLE=$ROOT/tests/data/sample-video.bin
EXPECTED=$ROOT/tests/data/sample-video-xbrz3.bin
SCALE=3

if [ ! -f "$SAMPLE" ] || [ ! -f "$EXPECTED" ]; then
    echo "xbrz: the sample or the expected frame is missing" >&2
    exit 1
fi

WORK=$ROOT/build/runs/xbrz/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

dotnet build "$ROOT/client" -c Release >/dev/null
dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" \
    --file "$SAMPLE" --headless --stats 0 --filter xbrz --scale $SCALE \
    --dump-frame "$WORK/frame.bin" >"$WORK/client.log" 2>&1

if [ ! -s "$WORK/frame.bin" ]; then
    echo "xbrz: FAIL - the client wrote no frame (see $WORK/client.log)" >&2
    exit 1
fi
if ! cmp -s "$EXPECTED" "$WORK/frame.bin"; then
    echo "xbrz: FAIL - the filtered frame differs from the reference" >&2
    cmp "$EXPECTED" "$WORK/frame.bin" | head -2 >&2
    exit 1
fi
echo "xbrz: OK (${SCALE}x, $(wc -c < "$WORK/frame.bin") bytes match the reference)"
echo "xbrz: artifacts in $WORK"
