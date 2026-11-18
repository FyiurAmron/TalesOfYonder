#!/bin/sh
# port-cancel gate: B2's prompt cancel (N/ESC) vs the B1 CANCEL-esc stop.
# Forks the port-combat flow at step 9 (prompt open): N returns to
# explore (SOUTH compass, silent side panel), matching B1's fn_0795E ax-0
# path (VGA wipe + explore repaint) bit-for-bit.
#
#   sh tests/port-cancel/run.sh
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/port-cancel/$STAMP"
FIX="$ROOT/tests/port-cancel"
COMBAT="$ROOT/tests/port-combat"

mkdir -p "$RUN"
sh "$ROOT/engine/port/tools/build.sh" >"$RUN/build.log" 2>&1
"$ROOT/build/port/yt2p-combat" "$COMBAT/spider.sav" \
    "$ROOT/_ref/orig-bin/YT2/PICTURES.VGA" \
    "$ROOT/_ref/orig-bin/YT2/WORLD.DAT" "$RUN/matrix" \
    --setup-text "$FIX/setup.txt" \
    --render-states "$FIX/render/01.txt:$FIX/render/02.txt:$FIX/render/03.txt:$FIX/render/04.txt:$FIX/render/05.txt:$FIX/render/06.txt:$FIX/render/07.txt:$FIX/render/08.txt:$FIX/render/09.txt:$FIX/render/10.txt" \
    --script "$FIX/script.txt" >"$RUN/tool.log" 2>&1
python3 "$FIX/check.py" "$RUN/matrix" >"$RUN/check.log" 2>&1
cat "$RUN/check.log"
