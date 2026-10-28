#!/bin/sh
# Starts the instrumented SWREG.EXE (seam build) under DOSBox-X.
#
# The link is the folder in server/game: the engine writes OUTPUT.BIN and
# reads INPUT.BIN, and client-start.sh follows them.  The script steps into
# server/ first: the bundled profile mounts "game" relative to it.
set -e
cd "$(dirname "$0")/server"

if ! command -v dosbox-x >/dev/null 2>&1; then
    echo "server-start: DOSBox-X not found on PATH."
    echo "  install it (https://dosbox-x.com/) or put dosbox-x in server/ and run:"
    echo "  PATH=\"$PWD:$PATH\" ./server-start.sh"
    exit 1
fi

echo "server-start: starting DOSBox-X (window title: SWREG)"
exec dosbox-x -nopromptfolder -fastlaunch -conf dosbox-x.conf
