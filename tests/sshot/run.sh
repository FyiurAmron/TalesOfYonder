#!/usr/bin/env bash
# Paired B1/B2 screenshot gate: B1 generates a deterministic save and
# captures each scenario step; B2 loads that save and captures the
# same steps live; compare.py requires byte-identical framebuffer
# indices (palettes may drift with animation). Exits nonzero on any
# index mismatch.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/build/runs/sshot"
rm -rf "$OUT"
mkdir -p "$OUT"
python3 "$ROOT/tests/sshot/b1.py" "$OUT" || exit 1
python3 "$ROOT/tests/sshot/b2.py" "$OUT" || exit 1
python3 "$ROOT/tests/sshot/compare.py" "$OUT" || exit 1
echo "sshot: OK (B1/B2 index-identical)"
