#!/bin/sh
# Replay a recording through both decoders and compare the renders.
#
#   sh tests/replay/run.sh [RECORDING]     (default: both tests/data/sample-*.bin)
#
# The C# client (--file --headless --ppm) and the Python reference decoder
# (tools/stream/replay.py --ppm) must produce byte-identical final frames: same
# framing, same palette expansion, same state.  This is the offline half of
# the client gate and needs neither DOSBox-X nor the game; tests/link/run.sh
# covers the live link.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
case "$#" in
    0) RECS="$ROOT/tests/data/sample-video.bin $ROOT/tests/data/sample-vbe.bin" ;;
    *) RECS="$1" ;;
esac
WORK=$ROOT/build/runs/replay/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"

dotnet build "$ROOT/client" -c Release >/dev/null
for REC in $RECS; do
    NAME=$(basename "$REC" .bin)
    dotnet "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" --file "$REC" --headless \
        --stats 0 --ppm "$WORK/$NAME-client.ppm"
    python3 "$ROOT/tools/stream/replay.py" "$REC" --out "$WORK/$NAME-python" \
        --ppm "$WORK/$NAME-python.ppm" --quiet

    if cmp -s "$WORK/$NAME-client.ppm" "$WORK/$NAME-python.ppm"; then
        echo "replay: OK ($(wc -c < "$WORK/$NAME-client.ppm") bytes of PPM, $NAME)"
    else
        echo "replay: FAIL - the two decoders disagree on $NAME" >&2
        echo "  client: $WORK/$NAME-client.ppm" >&2
        echo "  python: $WORK/$NAME-python.ppm" >&2
        exit 1
    fi
done
echo "replay: artifacts in $WORK"
