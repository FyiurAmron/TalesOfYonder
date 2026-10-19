#!/bin/sh
# Full gate for the SBFMDRV disassembly: assemble and byte-verify.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
sh "$SCRIPT_DIR/build.sh"
python3 "$SCRIPT_DIR/verify.py" "$ROOT/build/sbfmdrv/SBFMDRV.COM"
