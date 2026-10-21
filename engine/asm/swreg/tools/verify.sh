#!/bin/sh
# Full gate: pin check, assemble (build.sh) and byte-verify per instruction.
# Output goes to <repo>/build/swreg/oracle/SWREG.EXE.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
# The non-seam source is pinned (src/swreg.pin): an accidental edit outside
# the `IFDEF SEAM` blocks fails here until the pin is refreshed on purpose.
python3 "$SCRIPT_DIR/seam_pin.py" --check
sh "$SCRIPT_DIR/build.sh"
python3 "$SCRIPT_DIR/verify_asm.py" "$ROOT/build/swreg/oracle/SWREG.EXE"
