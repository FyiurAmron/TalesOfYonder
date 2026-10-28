# Seam builds: streaming the display out of the asm build

The seam is the integration surface: everything that talks to the outside world lives in
`engine/asm/swreg/src/seam.asm`, included by the game source under `IFDEF SEAM`, and the game source itself
only grows tagged, size-preserving hook sites (mostly a 5-byte `call far`).  The original display/input
sites are:

| hook | called from | does |
|---|---|---|
| `fn_SeamInit` | start-up, before the game allocates its own blocks | allocates the seam block, opens the backend |
| `fn_SeamPresent` | entry of `fn_PollInput` and of `fn_WaitKey` | polls RX, then emits the display diff (see below) |
| `fn_SeamDAC` | `fn_WriteDAC` (partial updates, `DS:SI`) | copies into the pending palette |
| `fn_SeamDACBlock` | `sub_SetDACBlock` (BIOS `1017h`, `ES:DX`) | same |
| `fn_SeamDAC768` | `sub_WriteDAC768` (intro loader, already 6-bit) | same |

(The other original site is the `include` of the seam file itself.)  The sound
path adds the OPL, speaker and SFX hooks described below; the engine and
speaker hooks carry the displaced instructions themselves because the game's
layout is hard-coded to the original function sizes.  The default build
compiles none of it: `sh engine/asm/swreg/tools/verify.sh` ends
`SWREG.EXE: OK` + `IDENTICAL`.

## Keeping the game source honest (`swreg.pin`)

A refactor here is easy to get wrong, and a silent edit to the game code would only surface much
later. `engine/asm/swreg/tools/seam_pin.py` strips the `IFDEF SEAM` / `IFDEF STREAM` blocks out of
`engine/asm/swreg/src/swreg.asm` and hashes the rest; the hash is pinned in `engine/asm/swreg/src/swreg.pin` and
`verify.sh` checks it **before** assembling. The baseline was pinned before the split
(`sha256 3435777…871e5`, 1,051,189 bytes / 46,160 stripped lines), so any accidental edit to game
code fails the gate:

    python3 engine/asm/swreg/tools/seam_pin.py --check     # exit 0 = the pinned core is untouched
    python3 engine/asm/swreg/tools/seam_pin.py --update    # only after a deliberate game-source change

## Builds

| script | defines | backend | output |
|---|---|---|---|
| `engine/asm/swreg/tools/build.sh` | — | none | `build/swreg/oracle/` (the oracle build; byte-identical) |
| `engine/asm/swreg/tools/build.sh --variant serial` | `-DSEAM` | COM1, 115200 8N1 (obsolete, kept for fun) | `build/swreg/serial/` |
| `engine/asm/swreg/tools/build.sh --variant file` | `-DSEAM -DSEAM_FILE` | timed `YT2B` records in `OUTPUT.BIN`/`INPUT.BIN` | `build/swreg/file/` |
| `engine/asm/swreg/tools/build.sh --variant tee` | `+ -DSEAM_TEE` | timed file plus raw COM1 mirror | `build/swreg/tee/` |
| `engine/asm/swreg/tools/build.sh --variant log` | `+ -DSEAM_LOG` | timed file + `SEAMLOG.TXT` records | `build/swreg/log/` |
| `engine/asm/swreg/tools/build.sh --variant fast` | `+ -DSEAM_FAST` | file, no presentation gate | `build/swreg/fast/` |

All of them run `fixhdr.py` and `check_mz_layout.py` (S1DEA prefix 7, `EMMXXXX0` delta `0C99h`)
and stage the minimum game files, so the output directory runs as-is.

    sh engine/asm/swreg/tools/build.sh --variant file
    sh tools/dosbox/stream.sh 45                      # run + replay, artifacts in build/runs/stream/…
    python3 tools/stream/replay.py OUTPUT.BIN --out frames/

`tools/stream/frames.py extract OUTPUT.BIN` decodes the same recording but prints one
line per distinct frame (PIT stamp, index-buffer hash, palette hash); `match` compares
that list with an engine `--present-log`. The M3 gate pins the settled intro frames of a
recording as checkpoints instead (see `engine/c/README.md`).

`STREAM_KEYS` injects XTEST keys at second marks relative to the DOSBox-X
launch, which is how the intro is skipped for a gameplay capture:

    STREAM_KEYS="62:Return 63:space 68:Return 69:space" sh tools/dosbox/stream.sh 95

`STREAM_SKIP_BUILD=1` reuses `build/swreg/file/` (useful when iterating on the
input script); the replay also counts `sample` messages when the recording
carries sound effects.

The product link is the file backend's folder transport (`OUTPUT.BIN`/`INPUT.BIN`, the C# client
with `--follow`); the `.BIN` files carry timestamps around unchanged v0 frames and are directly
replayable. It is documented in `docs/client.md`, together with the obsolete COM1/nullmodem link.

`fast` is a diagnostic: it drops the presentation budget in `fn_SeamPresent`, so a pass runs on
every hook call instead of at most three times per BIOS tick.  The budget exists because a poll or
wait-key loop calls the hook in a tight loop and a pass scans the whole screen, so the game can be
starved; `fast` is how the budget's cost is measured (the client gets two to five times the
pictures and the game's own window keeps its rate in the scenes measured so far), not a build to
hand out.  The budget is what lets every picture through: the pan is one screen row per picture
and the game advances it once per BIOS tick, so the client now gets all 18.2 of those a second
instead of 16-17, and scenes drawn faster than one picture per tick (the intro's fades, 36-53 a
second) are no longer held at the tick rate.

## What leaves the engine

Protocol v0, VIDEO family, codec `raw`, 320x200x8 for the game and 640x480x8 / 640x400x8 for the
VBE screens (publisher logo, title)
(`docs/protocol.md` has the layouts).

- **Present.** At most three passes per BIOS tick (55/s at most), in one of two modes:
  - mode 13h: walk the 200 rows, compare each against the 64,000-byte shadow and emit one 1-line
    `rect` per changed row; rows that did not change cost nothing.
  - VBE (640x480x8 detected with `4F03h` = 101h, and 640x400x8 = 100h for the title): the screen
    is 307,200 / 256,000 bytes and
    lives behind 64 KB VBE windows, so there is no second buffer. Each row is read through window
    A (`4F05h`, reprogrammed only when the bank changes) and compared with a 32-bit checksum kept
    per row; a changed row goes out as `rect(0,y,640,1)`. The checksum table sits in the mode-13h
    shadow area, which is unused while VBE is up. A pass sends at most ~16 KB of rows, so a full
    repaint spans several passes instead of stalling the game.
- **Mode changes.** The engine sends `set_mode` when it sees a different mode and expects the
  client to start from a blank screen; it does not resend a full frame for that reason. Entry into
  VBE clears the checksum table, entry into 13h forces a full pass.
- **Palette.** The DAC hooks never send: they copy the bytes the game is about to write into a
  pending 768-byte palette. A send takes the span from the first to the last entry that differs
  from the palette the client already has, is rate-limited to one send per BIOS tick (the present
  gate), and marks the frame dirty. If no staged write is pending, a game-context pass samples the
  DAC after a game palette write has established the current mode's palette; this catches writes
  outside the hooks. Tick-ISR passes also sample it when no staged write is pending, but only after
  two quiet ticks since the last palette hook and never while a hook is active. This lets the ISR
  catch unhooked changes without interrupting the game's DAC write. A mode change invalidates the
  baseline because BIOS silently loads its default palette; staged bytes are retained only when
  mode detection happened inside their hook. Forced keyframe sends also read the DAC, where the
  client's picture has been cleared.
- **Frames.** Ordinarily a pass with rows or palette changes ends with `frame_end`, including
  palette-only fade steps. A scene-sized palette jump with rows, or any palette change paired with
  at least 160 changed rows in mode 13h, gets one grace pass before `frame_end`. For the near-full
  repaint, the frame remains open for one bounded extra present pass, allowing a rapid palette
  sequence to reach its intended colors before presentation.
- **Keyframe.** A received CONTROL `keyframe_request` (or the obsolete serial `'K'`) asks for one:
  `set_mode`, the full palette, then a pass that treats every row as changed. It makes
  client/server start order irrelevant.

- **RX.** The link is polled from every present call, and inside the row scans (every 64 rows) so
  a long pass cannot starve input; while the transmitter is busy the UART's receive side is
  drained into a 32-byte ring which the next poll feeds through the parser.  That matters because
  the tee mirror can saturate COM1: a full RX FIFO silently drops host bytes.

Coalescing is what makes this cheap: a fade writes the DAC 18 times a second (up to ~56 calls per
tick in bursts) and coalesces to at most one span per present pass. A 45 s no-input run records
482 messages / 150,316 bytes; the pre-refactor policy (one full 256-entry palette message per fade
step) wrote 479,755 bytes for 90 s, and the two builds end pixel-identical.

## Block layout

One DOS block, `SEAM_BLK_PARAS 0x1040` (66,560 bytes), allocated in `fn_SeamInit`:

| offset | bytes | contents |
|---|---|---|
| `0000h` | 768 | pending palette (what the game last wrote) |
| `0300h` | 768 | sent palette (what the client has) |
| `0600h` | 832 | message buffer (14-byte header + up to 786 payload bytes) |
| `0940h` | 64,000 | shadow of the mode-13h screen |

If the allocation fails, `seam_blk` stays 0 and every hook is a no-op (the game runs unchanged).

## Implementation gotchas

- **Segment alignment is load-bearing.** Hook code before `S1DEA` shifts the data segment's
  paragraph prefix, and the game's hard-coded DS offsets depend on it (its `EMMXXXX0` check at
  `0CA0h` aborts with "Minimum of 1MB Expanded RAM must be available." when the prefix is not 7).
  The speaker hooks are size-preserving: a far call plus nop padding replaces the displaced PIT
  and gate instructions in `fn_SpeakerBeep`, and the stub runs those instructions itself, so
  `S06E7` keeps its size and every later segment keeps its offset. `check_mz_layout.py` refuses to
  stage a build whose data segment drifted.
- **The palette hook must read `DS:SI`, not `CS:SI`**: `fn_WriteDAC` uses `lodsb` and its callers
  only `push cs` so that its `retf` returns to them. The hook must also preserve the caller's
  flags/registers including `ES` (it is the palette source segment).
- **UASM symbols are case-insensitive**: a variable named `seam_tee` collides with the
  `-DSEAM_TEE` define and the expression silently loses its operand. Names avoid the defines.
- **A stalled link must not freeze the game**: the serial writer waits for THRE with a bounded
  counter and drops the rest of the message when it is exhausted.
- **Audio travels as register writes, not samples**: the seam patches the resident SBFMDRV's OPL
  writer (`sub_0A78`, a five-byte prologue at `0A78h`) with a far jump to `fn_SeamOplHook`.  The
  hook runs from the driver's timer ISR, appends `(u32 PIT stamp, register, value)` to a
  double-buffered log in the seam block (682 writes per buffer) and executes the five displaced
  bytes before returning to `0A7Dh`.  The stamp is the BIOS tick at `40:006Ch` times the driver's
  reload (`CS:0141h`) plus its tick phase (`CS:0147h`), read under `cli` so both halves come from
  one instant.  The driver zeroes the phase at every music start; the hook adds the lost part to
  the timeline (`seam_opl_adj`), so the stamps never step back and a new track keeps its tick
  spacing.
  A present pass, and the `fn_SeamPresentMusic` poll hook, drain one buffer into
  `opl_batch_timed` messages (1..128 writes per message, port from the driver's `0114h`) while
  `seam_inpresent` keeps the tick ISR out; the writes ride the same `OUTPUT.BIN` batch as the
  video and are flushed even when no frame closes.  The log is drained at the poll rate, so a
  batch can carry several driver ticks; the stamps keep their order and their ~10.4 ms spacing.
  `audio_ctrl(44100, 0, on)` mirrors the driver's `[02F2h]` state.  The
  driver's checksum only reads `[0101h]` at the first command 6, so it does
  not detect the patch, and `fn_SoundShutdown` sends command 7 (stop) before
  the game exits, so the resident driver never runs the patch into freed
  SWREG memory.  A 20 s file run carries 240 timed batches / 1,699 writes / 4
  controls over 353 frames / 2,073 rects / 328 palette messages, with no
  backwards stamp in the stream.
- **The PC-speaker beep bypasses the OPL log**: `fn_SpeakerBeep`'s PIT and
  gate writes are replaced by size-preserving far calls (plus nops) to
  `fn_SeamSpeakerOn`/`fn_SeamSpeakerOff`; the stubs execute the displaced
  instructions and send one `speaker(gate,divisor,pit)` message each, with
  the same stamp as the OPL writes.  This is effect 3's fallback when OPL
  sound effects are disabled, and it can sound while the sequencer is
  stopped.
- **The digitized effects bypass the dead far entry**: `fn_PlaySFX` normally
  needs `v_SoundEntry`, which SBFMDRV 1.34 does not provide.  The seam
  replaces its first six-byte `TEST` with a far call plus nop to
  `fn_SeamPlaySFX`, which sends `sample(id, offset, pit)` with the WORLD.DAT
  file offset from the game's own pointer table.  The caller's flags are
  preserved (the displaced TEST runs last), so the reference path is
  unchanged.  The client reads and mixes the VOC record; see
  `docs/client.md`.
- The block is allocated before `fn_07010`/`fn_01BE8`; it needs ~64 KB of conventional memory.

## Validation (2026-10-20)

- File backend, no input: 483 messages / 150,360 bytes (the first is the engine `hello`), 41
  frames, 402 rects, 38 palette messages, 1 control message.
- Live link (tee build, one session): the client received 478 of 482 messages (the first four
  predate its connection) and `tools/stream/compare_streams.py` reports them **byte-identical** to the
  seam's `OUTPUT.BIN`; the rendered final frame is pixel-identical to an independent decode of the
  same stream.
- Oracle build re-verified after every batch: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings; the pin
  gate is green.

Limitations: timing-dependent (the run length bounds what is captured), two
modes only (13h and the 640x480 and 640x400 VBE screens; any other VBE mode is
ignored), the PIT stamp is derived from the low BIOS-tick word (so it rolls
over about hourly and clients must use modular differences), and the seam
build is slower than the original.  The file backend reads input from
`INPUT.BIN` (the folder link); the obsolete serial backend has no input
channel.

## Timed folder records (2026-10-26)

The folder transport now stores each unchanged v0 frame inside a `YT2B` record:
an engine-PIT timestamp (`u32`), frame length (`u32`) and the complete frame.
Both files have a 16-byte header with version, PIT rate and direction. The ASM
file writer stamps `OUTPUT.BIN` with the same monotonic PIT timeline used by
AUDIO; its input reader validates the `INPUT.BIN` header and unwraps frames
before the existing protocol parser. The client writes timestamped input by
default, follows timed output without changing the live display path, and
`--file OUTPUT.BIN` replays output at its recorded pace. `--replay-input` also
accepts `INPUT.BIN`. Raw v0 fixtures and optional YT2S captures remain
compatible.

Validation: a 15 s DOSBox-X file run produced 1,849 timed records in 282,337
bytes; the first and last PIT stamps span 14.897 s, every frame decoded, and the
Python replay saved 291 frames. The deterministic bundled sample is checked by
`tests/session/make_sample.py --check`.
