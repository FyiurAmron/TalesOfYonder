#!/bin/sh
# Build the C core, the stub and the unit tests under build/c/.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
BUILD="$ROOT/build/c"
make -C "$ROOT/engine/c" BUILD="$BUILD" "$@"
echo "build: c -> $BUILD"
