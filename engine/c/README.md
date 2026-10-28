# engine/c

The Stage B C core and platform service layer. M2 (2026-10-28) is implemented: a C11
program can speak the same v0 / timed `YT2B` boundary as the asm seam, and the existing
C# client renders it.

## What is here

| Path | What |
|---|---|
| `include/yt2/` | the public core headers: memory, CPU flags/ALU, protocol, service, transports, video |
| `src/` | the core implementation and the `yt2stub` headless program |
| `tests/` | unit tests for memory, ALU/flags, framing/parsing, the service callbacks and video deltas |
| `tools/build.sh`, `tools/build.bat` | build entry points (gcc/clang and MSVC) |

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

    sh engine/c/tools/build.sh
    build/c/yt2core-tests
    sh tests/c/run.sh

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

## Deliberate M2 limits

- VIDEO codec 0 (raw rows) only; the run-length codec and scroll detection arrive with the
  video subsystem port. The AUDIO payload encoders exist, but no sound engine emits them yet.
- The stub scene is 320x200x8; VBE modes and the real game content are M3/M4 work.
- INPUT timestamps are validated as envelopes but applied when polled; timed scheduling of
  the C engine's own clock is part of the M3/M4 clock work.
- No SDL3 dependency. The existing C# client is the host; a direct SDL3 backend can be added
  behind the same transport/service pair later.
