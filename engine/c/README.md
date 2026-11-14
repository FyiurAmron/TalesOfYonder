# engine/c

**This tree is B1, the pseudo-port / executable spec. It is a milestone, not the
C deliverable.** The product engine will be B2: a hand-written, idiomatic C port with typed
game state and no asm, guest CPU/memory, segment dispatcher or emulation glue. See
`docs/porting.md` §3 and `docs/roadmap.md`.

M2/M3 (2026-10-28) implemented the platform service layer and the first emitted code; B1 now
runs the original to the title, the scripted M5 gameplay slice, save/load and sound, and
speaks the frozen v0 / timed `YT2B` boundary.  M7 (2026-11-11) completed the B1 milestone:
typed file/save records, protocol freeze and the Windows x64 cross-build.  The generated
game code is instruction-level C: every original instruction becomes C statements over
`g_m.r`, `g_m.mem` and a segment:offset dispatcher.  This is an emulator with the decode
hoisted to build time, kept only because it is the executable specification and the
differential oracle.

## What is here

| Path | What |
|---|---|
| `include/yt2/` | the public core headers: memory, CPU flags/ALU, `data.h` (typed records), protocol, service, transports, video, `game.h` |
| `src/` | the core implementation, `data.c`, `saveinfo.c` and the `yt2stub` headless program |
| `src/game/` | the hand-written boundary for the transliterated game (machine, services, `yt2game`) |
| `tests/` | unit tests for memory, the typed data model, ALU/flags, framing/parsing, the service callbacks, video deltas, string-instruction semantics, EMS mappings and the DOS arena |
| `tools/build.sh`, `tools/build-mingw.sh`, `tools/build.bat` | build entry points (Linux, Windows x64 cross-build, MinGW wrapper on Windows) |

The game code itself is emitted at build time into `build/c/generated/` by `tools/port/`
from the annotated UASM source; it is not part of this tree.

The layers, from the game outward:

- **memory**: one flat `0x110000`-byte array; `seg:off` is `seg*16+off`, so VGA RAM, the
  BIOS data area and the EMS window keep their original addresses. No bounds checks on
  purpose - the original code is the authority.
- **cpu**: explicit 286 registers and a flags word, with the exact CF/PF/AF/ZF/SF/OF rules
  for add/sub/inc/dec/logic/test/neg and the 16 condition codes. There is no runtime opcode
  decoder, but the emitted game functions are still the original instruction stream over
  this guest model; that is B1-only and must not appear in B2.
- **protocol**: v0 framing and payload builders/parsers for VIDEO, AUDIO, INPUT and CONTROL,
  plus the incremental INPUT parser (including the bare serial `'K'` keyframe request).
- **service**: the seam. It owns the output sequence, builds complete frames, answers the
  client hello once, turns INPUT/CONTROL frames into callbacks, and sends the `error` reply
  for an unknown family.
- **video**: the engine-side display seam: current mode, pixels, palette, and the client's
  per-row checksums. A present sends `set_mode` when needed, changed rows as codec-1
  `rect(0,y,w,1)` runs, a whole-screen `scroll(dy)` when the mode-13h pan detector fires,
  the changed palette span, and `frame_end`; a keyframe resends the mode, the whole palette
  and every row.
- **transport**: the only host-aware piece. `null` drops output, `stream` writes raw v0
  records, and `folder` writes timed `OUTPUT.BIN` and follows `INPUT.BIN` exactly like the
  asm file backend (16-byte `YT2B` header, one record per v0 frame, pseudo-ring wrap at
  64 MiB, input-header detection and rewind).  A live file can be recreated under the
  reader; a bad record makes the timed reader scan to the next plausible one (length plus
  the v0 frame magic) instead of failing, and a read error never disables the video link.

## Data model (B1 only)

`include/yt2/data.h` describes the original's on-disk records with fixed-width fields and
compile-time layout asserts: the seven `CURGAME`/`SAVEGAMEn` chunks, the party header and
nine 500-byte roster slots (including both 64-byte stat blocks, inventory, equipment and
containers), the DOS file struct, the picture-cache classes, the `CTMF` music block/patch
and the SFX table.  `src/data.c` owns the chunk table and the only file-bytes-to-struct
copies (`yt2_party_block_parse`/`write`).  `build/c/yt2saveinfo SAVEGAME` prints a save
through the model, and `tests/c/saveload.sh` runs it on the file the game's own disk panel
wrote.  The field-level reference is `docs/structures.md`.  These structs document the
file formats and feed the B1 tools; they are not the B2 game-state model, which B2 defines
around live party, map, object and combat state.

The host binaries are 64-bit on both Linux and Windows x64, but the B1 program state is
still the 16-bit guest model.  B2 has no such model.

## Build and run (B1)

The full B1 build needs `uasm` and `python3` with `capstone` (the game emission) besides
the C compiler; the core-only targets need just the compiler.

    sh engine/c/tools/build.sh
    build/c/yt2core-tests
    build/c/yt2game-tests   # emitted-code boundary semantics
    sh tests/c/run.sh
    sh tests/c/game.sh       # game run + M3 checkpoints (builds if stale)

Windows x64 cross-build (MinGW-w64, no MSVC needed; outputs `build/c-win/*.exe`):

    sh engine/c/tools/build-mingw.sh
    sh tests/c/windows.sh              # PE32+ check, Wine unit tests, Linux parity
    sh tests/c/windows.sh --full       # also the M5 + save/load run

`yt2game-tests` pins the parts of the CPU/DOS boundary the emitted code depends
on: `CMPS` reads `DS:SI` and subtracts `ES:DI` (the emitter's Capstone operand
order is easy to invert), a prefixed string op with `CX=0` is a no-op, the
shift helpers leave flags alone for count zero and apply the per-width
SHL/SHR/SAR overflow rule, IDIV reports `INT32_MIN / -1` as overflow, the
int 67h vector exposes the `EMMXXXX0` name the EMS probe compares, and the
EMS and DOS paragraph arenas allocate, map, coalesce and resize as the game
expects.

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
through an IRET sentinel and advances the BIOS tick word and midnight flag from the
emitted cycle clock.

    mkdir -p build/tmp/game build/tmp/checkpoint
    cp _ref/orig-bin/YT2/CURGAME _ref/orig-bin/YT2/WORLD.DAT _ref/orig-bin/YT2/PICTURES.VGA \
       _ref/orig-bin/YT2/CH2.BAT build/tmp/game/
    build/c/yt2game --game build/tmp/game --ticks 3600 --transfers 100000000 \
        --present-log build/tmp/present.txt --dump build/tmp/checkpoint
    python3 tests/c/game_frames.py build/tmp/present.txt \
        tests/c/game_checkpoints.txt --dump build/tmp/checkpoint

The C port presents at the seam's input-poll, tick-ISR, DAC and delay points and after
the full-screen work-buffer flush, with the seam's three passes per BIOS tick spread over
the tick, so `--present-log` gives the original's frame sequence (changed frames only).
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
    sh tools/play-c.sh --spelltext   # YT3-sourced F3 descriptions (92)
    sh tools/play-c.sh --spelltext-complete  # all 105 (13 curated lines)

By hand (two terminals):

    mkdir -p build/tmp/play
    cp _ref/orig-bin/YT2/CURGAME _ref/orig-bin/YT2/WORLD.DAT _ref/orig-bin/YT2/PICTURES.VGA \
       _ref/orig-bin/YT2/CH2.BAT build/tmp/play/
    build/c/yt2game --game build/tmp/play --live

    dotnet run --project client -c Release -- \
        --follow build/tmp/play/OUTPUT.BIN --input build/tmp/play/INPUT.BIN

`--live` means the link plus real-time pacing at the guest clock. The default is
3,000 cycles/ms (DOSBox-X's `cycles` are instructions per millisecond): about a 286
at 20-25 MHz, the class the game's intro and attract pacing were made for.
`--cycles-per-ms N` overrides it — 30,000 is roughly a 486DX2-66/DX4-100 and shows
how the original's timing collapses on a much faster machine. `--link` enables the
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

The C server presents mode 13h and both VBE intro modes (640x480 mode 101h,
640x400 mode 100h); the resident SBFMDRV path streams FM/PC-speaker audio by
default, and `--no-sound` keeps the engine silent (the checkpoint gates pin it
because the driver changes intro/script timing).
The original intro and attract run about 5.5 minutes before the title menu; that pacing is
the oracle's, not a stall. The menu accepts the mouse (the client maps the desktop pointer
to game pixels) and the keyboard.

The display link uses the seam's video codec 1 (row runs) and its whole-screen scroll
detector: a changed row travels as runs, and a mode-13h pan becomes one `scroll(dy)`
message plus the rows that slid in. The detector is the C port of
`sub_SeamScrollCheck` (+-8 rows, at least 32 changed rows and 100 matches); anything
else falls back to ordinary row rects. `tests/seam-rle` / `tests/seam-scroll` remain
the asm references, and `yt2core-tests` checks the encoder against their 640-pixel
rows and the pan rules.

## Deliberate limits

- VIDEO codec 1 (row runs) and whole-screen `scroll(dy)` detection are in place; LZ is
  still future. AUDIO messages carry the driver's OPL register batches with PIT stamps,
  its music control state and PC-speaker gate edges.
- The stub scene is 320x200x8; the game's VBE intro modes are presented, and
  `LOGO.PCX` is absent from `_ref/orig-bin/YT2/`, so the intro falls through to the
  title as it does in the oracle run.
- SBFMDRV is emulated, not executed: `game_sound.c` implements the resident INT API,
  timer ISR and sequencer from `docs/sbfmdrv.md`, and installs the signature and
  tables the game's vector scan looks for. The installer, far entry and SB IRQ path
  stay out of scope; the game's `v_SoundEntry` calls are no-ops (the driver has no
  such entry point).  The emitted `fn_PlaySFX` hook reports the game's authored
  WORLD.DAT effects as AUDIO sample records, mirroring the seam's `fn_SeamPlaySFX`;
  without it only the effect-3 PC-speaker fallback would be heard.
- The cycle model is DOSBox-X's `core=normal`: one cycle per instruction and one per
  REP iteration, charged while the copy runs, with the vertical sync pulse (two
  scanlines) as the port 3DAh retrace window. The intro's later waits run long under
  `--no-sound`; with the resident driver the music service cuts them short. The
  checkpoint gates pin `--no-sound` so the scripted runs keep the recorded timing.
  The settled checkpoint frames match, the four attract holds follow
  the original's 91-tick scene countdown (93 settled-to-next-change ticks, pinned in
  `tests/c/game_checkpoints.txt`), and the fade rate and the gate star are pinned by the
  timing model.
- INPUT timestamps are transport boundary time: the engine applies input when it polls
  the folder link, as the asm server does. Paced replay and timed input scheduling are the
  client's job (`--replay-input`); the engine's own clock stays logical and deterministic.
- No SDL3 dependency. The existing C# client is the host; a direct SDL3 backend can be added
  behind the same transport/service pair later.
