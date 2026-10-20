#!/bin/sh
# Full gate: assemble (via build.sh) and byte-verify per instruction against the reference.
# Output goes to <repo>/build/uasm/SWREG.EXE.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
sh "$SCRIPT_DIR/build.sh"
python3 "$SCRIPT_DIR/verify_asm.py" "$ROOT/build/uasm/SWREG.EXE"
