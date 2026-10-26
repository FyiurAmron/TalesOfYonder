#!/bin/sh
# Publish the client as a self-contained Windows x64 bundle.
#
#   sh client/publish-win.sh [options]
#
#     --sdl3 PATH         use this SDL3.dll instead of downloading the pinned
#                         release (its sha256 is still verified)
#     --server-dir DIR    seam build to stage as server/game
#                         (default: build/swreg/file, the folder-link build)
#     --out DIR           output root (default: build/dist)
#     --no-zip            leave the bundle directory unpacked only
#
# The bundle is self-contained: no .NET install is needed on the target, and
# SDL3.dll sits next to the exe (client/Sdl.cs loads "SDL3.dll" on Windows).
# Output: build/dist/yt2-win-x64/ and yt2-win-x64.zip.  See docs/client.md.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Pinned SDL3 runtime: the official win32-x64 archive (contains SDL3.dll).
SDL_VERSION=3.4.18
SDL_ZIP_NAME=SDL3-$SDL_VERSION-win32-x64.zip
SDL_ZIP_URL=https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/$SDL_ZIP_NAME
SDL_ZIP_SHA=75c2c0fc74e7d1206aaedc22893a35887332f4f85f5c8b74fddc10d177985f2c
SDL_DLL_SHA=2b4b128d8d510cc2d442693cf848c050d700d43dc667afe9e5fd82b0570d667e

SERVER_DIR=$ROOT/build/swreg/file
OUT=$ROOT/build/dist
ZIP=1
SDL3=

while [ $# -gt 0 ]; do
    case "$1" in
        --sdl3) SDL3=$2; shift 2 ;;
        --server-dir) SERVER_DIR=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        --no-zip) ZIP=0; shift ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "publish-win: unknown option $1" >&2; exit 2 ;;
    esac
done

if [ ! -f "$SERVER_DIR/SWREG.EXE" ]; then
    echo "publish-win: no seam build in $SERVER_DIR" >&2
    echo "  build it first:  sh engine/asm/swreg/tools/build.sh --variant file" >&2
    exit 1
fi
for f in CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    if [ ! -f "$SERVER_DIR/$f" ]; then
        echo "publish-win: $SERVER_DIR/$f is missing" >&2
        exit 1
    fi
done
SAMPLE=$ROOT/tests/data/sample-timed-stream.bin
if [ ! -f "$SAMPLE" ]; then
    echo "publish-win: $SAMPLE is missing (the timed BIN smoke-test stream)" >&2
    exit 1
fi
if ! python3 -c 'import struct,sys; d=open(sys.argv[1],"rb").read(16); sys.exit(0 if len(d)==16 and struct.unpack("<4sHHIB3x",d)==(b"YT2B",1,16,1193182,0) else 1)' "$SAMPLE"; then
    echo "publish-win: $SAMPLE is not an engine-to-client timed BIN stream" >&2
    exit 1
fi

CACHE=$ROOT/build/tmp/sdl3
mkdir -p "$CACHE"
if [ -n "$SDL3" ]; then
    DLL=$SDL3
else
    if [ ! -f "$CACHE/SDL3.dll" ]; then
        python3 - "$SDL_ZIP_URL" "$CACHE/$SDL_ZIP_NAME" "$SDL_ZIP_SHA" "$CACHE/SDL3.dll" "$SDL_DLL_SHA" <<'PY'
import hashlib, sys, urllib.request, zipfile
url, archive, want_zip, dll_out, want_dll = sys.argv[1:6]
data = urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "yt2-publish"}), timeout=300).read()
got = hashlib.sha256(data).hexdigest()
if got != want_zip:
    raise SystemExit("publish-win: %s sha256 %s != %s" % (url, got, want_zip))
open(archive, "wb").write(data)
with zipfile.ZipFile(archive) as z:
    blob = z.read("SDL3.dll")
got = hashlib.sha256(blob).hexdigest()
if got != want_dll:
    raise SystemExit("publish-win: SDL3.dll sha256 %s != %s" % (got, want_dll))
open(dll_out, "wb").write(blob)
print("publish-win: fetched SDL3 %s, SDL3.dll verified" % url.rsplit("/", 1)[-1])
PY
    fi
    DLL=$CACHE/SDL3.dll
fi
python3 - "$DLL" "$SDL_DLL_SHA" <<'PY'
import hashlib, sys
dll, want = sys.argv[1:3]
got = hashlib.sha256(open(dll, "rb").read()).hexdigest()
if got != want:
    raise SystemExit("publish-win: SDL3.dll sha256 %s != %s" % (got, want))
PY

BUNDLE=$OUT/yt2-win-x64
CLIENT=$BUNDLE/client
rm -rf "$BUNDLE"
mkdir -p "$CLIENT" "$BUNDLE/sample" "$BUNDLE/server/game" "$BUNDLE/server/capture"

echo "publish-win: publishing the client (win-x64, self-contained)"
dotnet publish "$ROOT/client" -c Release -r win-x64 --self-contained true \
    -p:PublishSingleFile=true -p:DebugType=none -o "$CLIENT" >/dev/null
cp "$DLL" "$CLIENT/SDL3.dll"
cp "$SCRIPT_DIR/publish/win/README.txt" "$BUNDLE/"
cp "$SCRIPT_DIR/publish/win/client-start.bat" "$BUNDLE/"
cp "$SCRIPT_DIR/publish/win/server-start.bat" "$BUNDLE/"
cp "$SCRIPT_DIR/publish/win/play.bat" "$BUNDLE/"
cp "$SCRIPT_DIR/publish/win/smoke-test.bat" "$BUNDLE/"
cp "$SAMPLE" "$BUNDLE/sample/STREAM.BIN"
for f in SWREG.EXE CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME; do
    cp "$SERVER_DIR/$f" "$BUNDLE/server/game/$f"
done
# Windows profile, not the Linux oracle/CI one: it sets
# "working directory option = noprompt" (DOSBox-X otherwise asks for a folder
# on every start) and has no autosave/logging leftovers.
cp "$SCRIPT_DIR/publish/win/dosbox-x-server.conf" "$BUNDLE/server/dosbox-x.conf"

# cmd.exe is unreliable with LF-only batch files; normalise every staged .bat
# so an editor that writes LF cannot ship a broken launcher.
python3 - "$BUNDLE" <<'PY'
import os, sys
root = sys.argv[1]
for base, _, files in os.walk(root):
    for name in files:
        if name.lower().endswith(".bat"):
            path = os.path.join(base, name)
            data = open(path, "rb").read().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
            open(path, "wb").write(data)
PY

if [ "$ZIP" = 1 ]; then
    python3 - "$BUNDLE" "$OUT/yt2-win-x64.zip" <<'PY'
import hashlib, os, sys, zipfile
bundle, zpath = sys.argv[1:3]
top = os.path.basename(bundle)
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for base, dirs, files in os.walk(bundle):
        # empty folders are part of the layout (server\capture), so store them
        for name in sorted(dirs):
            path = os.path.join(base, name)
            z.write(path, os.path.join(top, os.path.relpath(path, bundle)))
        for name in sorted(files):
            path = os.path.join(base, name)
            z.write(path, os.path.join(top, os.path.relpath(path, bundle)))
print("publish-win: %s (%.1f MB)" % (zpath, os.path.getsize(zpath) / 1e6))
print("publish-win: sha256 %s" % hashlib.sha256(open(zpath, "rb").read()).hexdigest())
PY
fi
echo "publish-win: bundle in $BUNDLE"
