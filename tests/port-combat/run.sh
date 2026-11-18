#!/bin/sh
# port-combat gate: the B2 S/A combat rounds vs B1 quiescent goldens.
# Builds the port, runs yt2p-combat over the spider-staging save
# (S/S/S/A/A to quiescence each), and pins per-action combat bytes
# (seed, flags, HP, positions, XP), the cumulative SFX log, the
# palette hash, and the masked video hash.
#
#   sh tests/port-combat/run.sh
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/port-combat/$STAMP"
FIX="$ROOT/tests/port-combat"

mkdir -p "$RUN"
sh "$ROOT/engine/port/tools/build.sh" >"$RUN/build.log" 2>&1
"$ROOT/build/port/yt2p-combat" "$FIX/spider.sav" \
    "$ROOT/_ref/orig-bin/YT2/PICTURES.VGA" \
    "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" "$RUN/matrix" \
    --setup-text "$FIX/setup.txt" \
    --render-states "$FIX/render/01.txt:$FIX/render/02.txt:$FIX/render/03.txt:$FIX/render/04.txt:$FIX/render/05.txt:$FIX/render/06.txt:$FIX/render/07.txt:$FIX/render/08.txt:$FIX/render/09.txt:$FIX/render/10.txt:$FIX/render/11.txt:$FIX/render/12.txt:$FIX/render/13.txt:$FIX/render/14.txt:$FIX/render/15.txt:$FIX/render/16.txt" \
    --script "$FIX/script.txt" >"$RUN/tool.log" 2>&1
python3 "$FIX/check.py" "$RUN/matrix" >"$RUN/check.log" 2>&1
cat "$RUN/check.log"
