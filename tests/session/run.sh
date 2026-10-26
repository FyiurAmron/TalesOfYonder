#!/bin/sh
# Capture and replay a timed, bidirectional client/server session.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$ROOT/build/runs/session/$(date +%Y%m%d-%H%M%S)
mkdir -p "$WORK"
dotnet build "$ROOT/client" -c Release >/dev/null
python3 "$ROOT/tests/session/make_sample.py" --check
python3 "$ROOT/tests/session/check.py" \
    "$ROOT/build/client/bin/Release/net11.0/Yt2Client.dll" "$WORK"
