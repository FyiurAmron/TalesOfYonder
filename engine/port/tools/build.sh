#!/bin/sh
# Build the B2 real C port.  Outputs go to build/port/ (override with BUILD=).
set -eu
PORT=$(cd "$(dirname "$0")/.." && pwd)
exec make -C "$PORT" all
