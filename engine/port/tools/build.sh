#!/bin/sh
# Build the B2 real C port: the player and the unit tests by default
# (all), every dev tool with "tools", one tool by its short name.
# Outputs go to build/port/ (override with BUILD=).
set -eu
PORT=$(cd "$(dirname "$0")/.." && pwd)
exec make -C "$PORT" ${@:-all}
