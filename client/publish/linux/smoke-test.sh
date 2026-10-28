#!/bin/sh
# Quick check without DOSBox-X: replays the bundled timed BIN stream in a window and
# writes sample/smoke.ppm (320x200 PPM of the last frame).  Exit code 0 means
# the client binary, SDL3, the window and the decoder are fine.
set -e
cd "$(dirname "$0")"

client/Yt2Client --file sample/OUTPUT.BIN --ppm sample/smoke.ppm
echo "smoke-test: OK, last frame in sample/smoke.ppm"
