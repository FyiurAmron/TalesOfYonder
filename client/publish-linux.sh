#!/bin/sh
# Publish the client as a Linux x64 bundle with the seam server.
#
#   sh client/publish-linux.sh [options]
#
#     --server-dir DIR    seam build to stage as server/game
#                         (default: build/swreg/file, the folder-link build)
#     --out DIR           output root (default: build/dist)
#     --framework         framework-dependent publish (needs .NET 11 on the
#                         target); the default is self-contained
#     --no-tar            leave the bundle directory unpacked only
#
# The bundle is ready to run: server-start.sh opens DOSBox-X, and
# client-start.sh (or play.sh, which starts both) mirrors the game.
# Output: build/dist/yt2-linux-x64/ and yt2-linux-x64.tar.gz.  See docs/client.md.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SERVER_DIR=$ROOT/build/swreg/file
OUT=$ROOT/build/dist
TAR=1
SELF_CONTAINED=1

while [ $# -gt 0 ]; do
    case "$1" in
        --server-dir) SERVER_DIR=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        --framework) SELF_CONTAINED=0; shift ;;
        --no-tar) TAR=0; shift ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "publish-linux: unknown option $1" >&2; exit 2 ;;
    esac
done

if [ ! -f "$SERVER_DIR/SWREG.EXE" ]; then
    echo "publish-linux: no seam build in $SERVER_DIR" >&2
    echo "  build it first:  sh engine/asm/swreg/tools/build.sh --variant file" >&2
    exit 1
fi
for f in CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    if [ ! -f "$SERVER_DIR/$f" ]; then
        echo "publish-linux: $SERVER_DIR/$f is missing" >&2
        exit 1
    fi
done
SAMPLE=$ROOT/tests/data/sample-timed-stream.bin
if [ ! -f "$SAMPLE" ]; then
    echo "publish-linux: $SAMPLE is missing (the timed BIN smoke-test stream)" >&2
    exit 1
fi
if ! python3 -c 'import struct,sys; d=open(sys.argv[1],"rb").read(16); sys.exit(0 if len(d)==16 and struct.unpack("<4sHHIB3x",d)==(b"YT2B",1,16,1193182,0) else 1)' "$SAMPLE"; then
    echo "publish-linux: $SAMPLE is not an engine-to-client timed BIN stream" >&2
    exit 1
fi

BUNDLE=$OUT/yt2-linux-x64
CLIENT=$BUNDLE/client
rm -rf "$BUNDLE"
mkdir -p "$CLIENT" "$BUNDLE/server/game" "$BUNDLE/server/capture" "$BUNDLE/sample"

echo "publish-linux: publishing the client"
if [ "$SELF_CONTAINED" = 1 ]; then
    dotnet publish "$ROOT/client" -c Release -r linux-x64 --self-contained true \
        -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true \
        -p:DebugType=none -o "$CLIENT" >/dev/null
else
    dotnet publish "$ROOT/client" -c Release -o "$CLIENT" >/dev/null
fi

# SDL3 is loaded by name; keep a copy next to the client so the bundle does
# not depend on the distribution's SDL packaging.
SDL_LIB=$(ldconfig -p 2>/dev/null | awk '/libSDL3\.so\.0 .*x86-64/ { print $NF; exit }')
if [ -z "$SDL_LIB" ]; then
    SDL_LIB=$(ls /usr/lib/x86_64-linux-gnu/libSDL3.so.0 2>/dev/null | head -1)
fi
if [ -n "$SDL_LIB" ] && [ -f "$SDL_LIB" ]; then
    cp -f "$SDL_LIB" "$CLIENT/libSDL3.so.0"
else
    echo "publish-linux: warning: no libSDL3.so.0 found; the target needs SDL3" >&2
fi

for f in SWREG.EXE CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp -f "$SERVER_DIR/$f" "$BUNDLE/server/game/$f"
done
cp -f "$ROOT/client/publish/linux/dosbox-x.conf" "$BUNDLE/server/dosbox-x.conf"
cp -f "$ROOT/client/publish/linux/server-start.sh" "$BUNDLE/server-start.sh"
cp -f "$ROOT/client/publish/linux/client-start.sh" "$BUNDLE/client-start.sh"
cp -f "$ROOT/client/publish/linux/play.sh" "$BUNDLE/play.sh"
cp -f "$ROOT/client/publish/linux/smoke-test.sh" "$BUNDLE/smoke-test.sh"
cp -f "$ROOT/client/publish/linux/README.txt" "$BUNDLE/README.txt"
cp -f "$SAMPLE" "$BUNDLE/sample/STREAM.BIN"
chmod +x "$BUNDLE/client-start.sh" "$BUNDLE/server-start.sh" \
         "$BUNDLE/play.sh" "$BUNDLE/smoke-test.sh"
[ -f "$CLIENT/Yt2Client" ] && chmod +x "$CLIENT/Yt2Client"

if [ "$TAR" = 1 ]; then
    tar -C "$OUT" -czf "$OUT/yt2-linux-x64.tar.gz" yt2-linux-x64
    echo "publish-linux: $OUT/yt2-linux-x64.tar.gz"
fi
echo "publish-linux: $BUNDLE"
