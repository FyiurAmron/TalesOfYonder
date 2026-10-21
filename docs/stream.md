# Seam builds: streaming the display out of the asm build

The seam is the integration surface: everything that talks to the outside world lives in
`engine/asm/swreg/src/seam.asm`, included by the game source under `IFDEF SEAM`, and the game source itself
only grows **six tagged hook sites** (a 5-byte `call far` each):

| hook | called from | does |
|---|---|---|
| `fn_SeamInit` | start-up, before the game allocates its own blocks | allocates the seam block, opens the backend |
| `fn_SeamPresent` | entry of `fn_PollInput` and of `fn_WaitKey` | polls RX, then emits the display diff (see below) |
| `fn_SeamDAC` | `fn_WriteDAC` (partial updates, `DS:SI`) | copies into the pending palette |
| `fn_SeamDACBlock` | `sub_SetDACBlock` (BIOS `1017h`, `ES:DX`) | same |
| `fn_SeamDAC768` | `sub_WriteDAC768` (intro loader, already 6-bit) | same |

(The sixth site is the `include` of the seam file itself.) The default build compiles none of it:
`sh engine/asm/swreg/tools/verify.sh` ends `SWREG.EXE: OK` + `IDENTICAL`.

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
| `engine/asm/swreg/tools/build.sh --variant serial` | `-DSEAM` | COM1, 115200 8N1 | `build/swreg/serial/` |
| `engine/asm/swreg/tools/build.sh --variant file` | `-DSEAM -DSEAM_FILE` | `STREAM.BIN` in the game directory | `build/swreg/file/` |
| `engine/asm/swreg/tools/build.sh --variant tee` | `+ -DSEAM_TEE` | both at once (file is the ground truth) | `build/swreg/tee/` |
| `engine/asm/swreg/tools/build.sh --variant log` | `+ -DSEAM_LOG` | file + `SEAMLOG.TXT` records | `build/swreg/log/` |

All of them run `fixhdr.py` and `check_mz_layout.py` (S1DEA prefix 7, `EMMXXXX0` delta `0C99h`)
and stage the minimum game files, so the output directory runs as-is.

    sh engine/asm/swreg/tools/build.sh --variant file
    sh tools/dosbox/stream.sh 45                      # run + replay, artifacts in build/runs/stream/…
    python3 tools/stream/replay.py STREAM.BIN --out frames/

The live link (`engine/asm/swreg/tools/build.sh --variant serial`, `tools/dosbox/dosbox-x-client.conf`,
the C# client) is documented in
`docs/client.md`.

## What leaves the engine

Protocol v0, VIDEO family, codec `raw`, 320x200x8 for the game and 640x480x8 / 640x400x8 for the
VBE screens (publisher logo, title)
(`docs/protocol.md` has the layouts).

- **Present.** At most once per two BIOS ticks, in one of two modes:
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
- **Keyframe.** A received `'K'` (0x4B) asks for one: `set_mode`, the full palette, then a pass
  that treats every row as changed. It makes client/server start order irrelevant.

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

- **Segment alignment is load-bearing.** The hooks add 30 bytes of code before `S1DEA`, which
  would move the data segment's paragraph prefix from 7 to 5 and break the game's hard-coded DS
  offsets (its `EMMXXXX0` check at `0CA0h` aborts with "Minimum of 1MB Expanded RAM must be
  available."). A 2-byte pad at the end of `S1DC9` restores prefix 7; `check_mz_layout.py` refuses
  to stage a drifted build.
- **The palette hook must read `DS:SI`, not `CS:SI`**: `fn_WriteDAC` uses `lodsb` and its callers
  only `push cs` so that its `retf` returns to them. The hook must also preserve the caller's
  flags/registers including `ES` (it is the palette source segment).
- **UASM symbols are case-insensitive**: a variable named `seam_tee` collides with the
  `-DSEAM_TEE` define and the expression silently loses its operand. Names avoid the defines.
- **A stalled link must not freeze the game**: the serial writer waits for THRE with a bounded
  counter and drops the rest of the message when it is exhausted.
- The block is allocated before `fn_07010`/`fn_01BE8`; it needs ~64 KB of conventional memory.

## Validation (2026-10-20)

- File backend, no input: 483 messages / 150,360 bytes (the first is the engine `hello`), 41
  frames, 402 rects, 38 palette messages, 1 control message.
- Live link (tee build, one session): the client received 478 of 482 messages (the first four
  predate its connection) and `tools/stream/compare_streams.py` reports them **byte-identical** to the
  seam's `STREAM.BIN`; the rendered final frame is pixel-identical to an independent decode of the
  same stream.
- Oracle build re-verified after every batch: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings; the pin
  gate is green.

Limitations: timing-dependent (the run length bounds what is captured), two modes only (13h and
the 640x480 and 640x400 VBE screens; any other VBE mode is ignored), no audio
yet, and the seam build is slower than the original.  The file backend does not read input (no
serial link); keys arrive only on the live link.
