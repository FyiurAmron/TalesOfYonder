#!/bin/sh
# Check the intro presentation against the recording: the logo fades must be
# ramps (one pass per DAC write), the twinkling title star must animate, and
# the story text must keep the full palette instead of a stale 4124h block.
#
#   sh tests/c/intro.sh              # build if needed, run 1300 ticks
#   sh tests/c/intro.sh --no-build   # use build/c/yt2game as it is
#   sh tests/c/intro.sh --build      # force a rebuild
#
# Artifacts land in build/runs/intro/<stamp>/; the frames are deleted after
# the check, the presentation log stays.  Play the same build with
# `sh tools/play-c.sh` to watch it through the client.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/c"
STAMP=$(date +%Y%m%d-%H%M%S)-$$
RUN="$ROOT/build/runs/intro/$STAMP"
GAME="$BUILD/yt2game"
DO_BUILD=auto
TICKS=1300

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) DO_BUILD=no; shift ;;
        --build) DO_BUILD=yes; shift ;;
        --ticks) TICKS=${2:?--ticks needs a value}; shift 2 ;;
        *)
            echo "intro: unknown option $1" >&2
            exit 2
            ;;
    esac
done

if [ "$DO_BUILD" = yes ] || { [ "$DO_BUILD" = auto ] && [ ! -x "$GAME" ]; }; then
    sh "$ROOT/engine/c/tools/build.sh"
fi

mkdir -p "$RUN/game" "$RUN/frames"
for f in CURGAME WORLD.DAT PICTURES.VGA CH2.BAT; do
    cp "$ROOT/_ref/orig-bin/$f" "$RUN/game/$f"
done

"$GAME" --game "$RUN/game" --ticks "$TICKS" --transfers 400000000 \
    --present-log "$RUN/present.txt" --present-dir "$RUN/frames" \
    >"$RUN/stdout.log" 2>"$RUN/stderr.log"

python3 - "$RUN" <<'PY'
import pathlib
import sys

run = pathlib.Path(sys.argv[1])
frames = run / "frames"


def fnv(data):
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def palette(frame):
    return (frames / f"frame_{frame:05d}.pal").read_bytes()


def nonzero(colors):
    return sum(1 for entry in range(256)
               if any(colors[entry * 3:entry * 3 + 3]))


def index(frame):
    return (frames / f"frame_{frame:05d}.idx").read_bytes()


fails = []
available = sorted(int(path.stem.split("_")[1]) for path
                   in frames.glob("frame_*.pal"))
if not available:
    raise SystemExit("intro: the run produced no frames")

# 1. The Webfoot logo fade is a ramp.  Without present-on-DAC it reached the
#    screen in one or two steps; the fix gives one step per fade pass.
logo = {fnv(palette(frame)) for frame in available if frame < 40}
if len(logo) < 8:
    fails.append(f"logo fade has only {len(logo)} palette steps (want >= 8)")

# 2. The title star twinkles through the DAC (the stars are static pixels
#    whose palette entry ramps).  Sample one known star, (206,5).
star = []
for frame in available:
    if 88 <= frame <= 118:
        colors = palette(frame)
        value = index(frame)[5 * 320 + 206]
        star.append(tuple(colors[value * 3:value * 3 + 3]))
steps = len(set(star))
if steps < 5:
    fails.append(f"star at (206,5) shows {steps} colours (want >= 5)")

# 3. The story picture must keep the full palette: the pre-fix run restored a
#    stale (mostly black) v_PalCurrent block right after the text appeared.
story_hash = 0x9504AC7891CAB64E
story = next((frame for frame in available
              if fnv(index(frame)) == story_hash), None)
if story is None:
    fails.append("the story-text picture never appeared")
else:
    for frame in range(story, story + 9):
        if frame not in available:
            fails.append(f"the story picture ended at frame {frame}")
            break
        count = nonzero(palette(frame))
        if count < 200:
            fails.append(
                f"story frame {frame} has {count} palette entries (want >= 200)")
            break

if fails:
    for fail in fails:
        print(f"intro: {fail}", file=sys.stderr)
    raise SystemExit(1)
print(f"intro: OK (logo fade {len(logo)} steps, star {steps} colours, "
      f"story palette {nonzero(palette(story))} entries at frame {story})")
PY

# Keep the log but not 65 MiB of frames.
rm -rf "$RUN/frames"
echo "intro: artifacts in $RUN"
