# engine/c

The Stage B C core and platform service layer. M2 and M3 (2026-10-28) are implemented: a
C11 program speaks the same v0 / timed `YT2B` boundary as the asm seam and the existing
C# client renders it, the transliterated original reaches the title/attract with
checkpoint hashes matching the oracle, and the same engine is playable through the live
folder link (`--live`).

## What is here

| Path | What |
|---|---|
| `include/yt2/` | the public core headers: memory, CPU flags/ALU, protocol, service, transports, video, `game.h` |
| `src/` | the core implementation and the `yt2stub` headless program |
| `src/game/` | the hand-written boundary for the transliterated game (machine, services, `yt2game`) |
| `tests/` | unit tests for memory, ALU/flags, framing/parsing, the service callbacks, video deltas and the game's string-instruction semantics |
| `tools/build.sh`, `tools/build.bat` | build entry points (gcc/clang and MSVC) |

The game code itself is emitted at build time into `build/c/generated/` by `tools/port/`
from the annotated UASM source; it is not part of this tree.

The layers, from the game outward:

- **memory**: one flat `0x110000`-byte array; `seg:off` is `seg*16+off`, so VGA RAM, the
  BIOS data area and the EMS window keep their original addresses. No bounds checks on
  purpose - the original code is the authority.
- **cpu**: explicit 286 registers and a flags word, with the exact CF/PF/AF/ZF/SF/OF rules
  for add/sub/inc/dec/logic/test/neg and the 16 condition codes. There is no interpreter;
  the transliterated game functions do their own arithmetic through these helpers.
- **protocol**: v0 framing and payload builders/parsers for VIDEO, AUDIO, INPUT and CONTROL,
  plus the incremental INPUT parser (including the bare serial `'K'` keyframe request).
- **service**: the seam. It owns the output sequence, builds complete frames, answers the
  client hello once, turns INPUT/CONTROL frames into callbacks, and sends the `error` reply
  for an unknown family.
- **video**: the engine-side display seam: current mode, pixels, palette, and a shadow of
  what the client has. A present sends `set_mode` when needed, changed rows as
  `rect(0,y,w,1)`, the changed palette span, and `frame_end`; a keyframe resends the mode,
  the whole palette and every row.
- **transport**: the only host-aware piece. `null` drops output, `stream` writes raw v0
  records, and `folder` writes timed `OUTPUT.BIN` and follows `INPUT.BIN` exactly like the
  asm file backend (16-byte `YT2B` header, one record per v0 frame, pseudo-ring wrap at
  64 MiB, input-header detection and rewind).

## Build and run

The full build needs `uasm` and `python3` with `capstone` (the game emission) besides the
C compiler; the M2-only targets need just the compiler.

    sh engine/c/tools/build.sh
    build/c/yt2core-tests
    build/c/yt2game-tests   # string-instruction pointer semantics
    sh tests/c/run.sh
    sh tests/c/game.sh       # game run + M3 checkpoints (builds if stale)

The gate builds both binaries under `build/c/`, runs the unit tests, re-emits
`tests/data/sample-video.bin` through the C folder backend and requires the unwrapped C
frames to match the asm recording byte for byte, renders the stub output with both the C#
client and `tools/stream/replay.py` and requires identical PPMs, then drives the stub with
scripted `INPUT.BIN` files (keyframe, key, mouse, quit, live append) and checks the stream
and null backends.

Direct use:

    build/c/yt2stub --transport folder \
        --output build/runs/demo/OUTPUT.BIN --input build/runs/demo/INPUT.BIN \
        --frames 24

    dotnet run --project client -c Release -- \
        --file build/runs/demo/OUTPUT.BIN --headless --no-audio --ppm frame.ppm

`--replay-v0 PATH` re-emits a raw v0 recording after re-encoding every message through the
C builders and checking each payload is byte-identical; it is the ASM/C seam-parity check.
`--input-start` makes the folder transport read an existing `INPUT.BIN` from the beginning
(the live default is the asm behaviour: start at the current end and consume new records).

## Transliterated game (M3)

`engine/c/tools/build.sh` assembles `engine/asm/swreg/src/swreg.asm` with UASM and runs
`tools/port/emit_c.py`, which emits one dispatch unit per `(segment, offset)` into
`build/c/generated/`. The boundary in `src/game/` boots the relocated original image at
`YT2_LOAD_SEG`, services DOS/BIOS/EMS/mouse/clock calls, runs the INT 1Ch handler nested
through an IRET sentinel and advances the BIOS tick word from the emitted cycle clock.

    mkdir -p build/tmp/game build/tmp/checkpoint
    cp _ref/orig-bin/CURGAME _ref/orig-bin/WORLD.DAT _ref/orig-bin/PICTURES.VGA \
       _ref/orig-bin/CH2.BAT build/tmp/game/
    build/c/yt2game --game build/tmp/game --ticks 2500 --transfers 100000000 \
        --present-log build/tmp/present.txt --dump build/tmp/checkpoint
    python3 tests/c/game_frames.py build/tmp/present.txt \
        tests/c/game_checkpoints.txt --dump build/tmp/checkpoint

The SEAM build presents at input-poll and tick-ISR entry, the same points `yt2game`
samples, so `--present-log` gives the original's frame sequence (changed frames only).
The checker requires seven settled intro scenes (Webfoot logo, two story screens, four
attract vignettes) in order, each held for at least 60 ticks, with framebuffer and DAC
hashes matching the stream recording. The end dump is re-hashed and the settled globals
(`v_94F1`, `v_94D7`, `v_50F6`) are compared with the DOSBox-X save states.

`--present-dir DIR` additionally writes the raw index buffer and 6-bit palette of every
changed frame (`frame_NNNNN.idx`/`.pal`), which is how new checkpoints are inspected.
`--ppm FILE` writes the last presented frame as 8-bit RGB.

## Live play (folder link)

`--live` runs the game in real time and connects it to the C# client through the same
`OUTPUT.BIN`/`INPUT.BIN` pair as the asm seam build. From the tree, one command stages
the game data, builds what is stale, starts the engine and opens the client:

    sh tools/play-c.sh                # client window
    sh tools/play-c.sh --scale 4
    sh tools/play-c.sh --headless --stats 5

By hand (two terminals):

    mkdir -p build/tmp/play
    cp _ref/orig-bin/CURGAME _ref/orig-bin/WORLD.DAT _ref/orig-bin/PICTURES.VGA \
       _ref/orig-bin/CH2.BAT build/tmp/play/
    build/c/yt2game --game build/tmp/play --live

    dotnet run --project client -c Release -- \
        --follow build/tmp/play/OUTPUT.BIN --input build/tmp/play/INPUT.BIN

`--live` means the link plus pacing at the oracle profile's 30,000 cycles/ms, so the
BIOS tick and the game's animations run at the original speed. `--link` enables the
folder link without pacing (fast, used by the transport tests); `--no-pace` with
`--live` does the same. `--transfers 0` (the default) runs without a dispatch cap.
The client asks for a keyframe when it connects, so a client that starts late
resynchronises.

Input is delivered exactly where the game looks for it: key events go into the DOS
type-ahead queue drained by INT 21h 06h (with the zero-byte/scancode form for extended
keys and the 0Ch flush), and mouse motion/buttons drive the INT 33h shim and the game's
own `fn_MouseHandler`. `--trace-input` logs the traffic for debugging.

The whole thing ships as a bundle without DOSBox-X:

    sh engine/c/tools/build.sh
    sh tools/publish-linux.sh --server c
    build/dist/yt2-linux-x64/play.sh

The C server presents mode 13h only for now: the VBE intro modes wait for M4 and the
FM/PC-speaker stream waits for M6, so the engine is silent and skips VBE-only pictures.
The original intro and attract run about 5.5 minutes before the title menu; that pacing is
the oracle's, not a stall. The menu accepts the mouse (the client maps the desktop pointer
to game pixels) and the keyboard.

## Deliberate M2/M3 limits

- VIDEO codec 0 (raw rows) only; the run-length codec and scroll detection arrive with the
  video subsystem port. The AUDIO payload encoders exist, but no sound engine emits them yet.
- The stub scene is 320x200x8; VBE modes beyond the game's `4F02h 101h` probe wait for
  M4, and `LOGO.PCX` is absent from `_ref/orig-bin/`, so the intro falls through to the
  title as it does in the oracle run.
- SBFMDRV is not resident yet (M6): the game's sound calls are accepted but no OPL
  stream is produced.
- The cycle model is a flat 2 cycles per emitted instruction plus the VGA retrace
  window; exact per-opcode costs and sub-tick scheduling wait for M4. The intro's early
  Webfoot-to-title palette cross-fade therefore lands on a different loop iteration,
  while the settled checkpoint frames match.
- INPUT timestamps are validated as envelopes but applied when polled; timed scheduling of
  the C engine's own clock is part of M4.
- No SDL3 dependency. The existing C# client is the host; a direct SDL3 backend can be added
  behind the same transport/service pair later.
