# STREAM build (M1b boundary spike)

Experimental instrumentation that streams the mode-13h screen and palette out of the annotated
asm build into the v0 VIDEO message format. It proves the protocol framing and the showcase path
(the recorded frames replay to the same picture as the emulator's screen) without touching the
byte-perfect oracle build.

> The default build is unchanged: every addition is under `IFDEF STREAM`, and `-DSTREAM` is only
> passed by `build-stream.sh`. `sh asm/tools/verify.sh` still reports `SWREG.EXE: OK` +
> `IDENTICAL`. The STREAM EXE is **not** byte-identical by design.

## Build and run

    sh asm/tools/build-stream.sh                 # -> build/stream/SWREG.EXE + game files
    sh harness/stream.sh [seconds]               # default 25; builds, runs, replays
    python3 harness/stream_client.py STREAM.BIN --out frames/

`harness/stream.sh` stages the instrumented build in `build/stream/run/<timestamp>/game/`, runs
DOSBox-X headless on Xvfb `:97` with the oracle profile (`harness/dosbox-x.conf`), saves one
emulator snapshot at 15 s, then moves `STREAM.BIN` to the run directory and replays it. Artifacts:

| path | contents |
|---|---|
| `STREAM.BIN` | bare v0 VIDEO message stream (no session header yet) |
| `capture/screen_015.png` | emulator screen at 15 s, for a side-by-side check |
| `frames/frame_*.png` | one image per content frame (`frame_end`), palette applied |
| `frames/final.png` | last screen with the last palette (fades included) |
| `run.log`, `stdout.log` | DOSBox-X logs |

## What is recorded

Only the VIDEO family, raw codec, 320x200x8. The layouts are pinned in `docs/protocol.md`
(`set_mode`, `palette`, `rect`, `frame_end`). Values are 6-bit DAC entries; the client expands
them to 8-bit for display.

- **Present** (`fn_StreamPresent`): called at the entry of the two input-poll routines
  (`fn_0D0FE`, `fn_06342`), i.e. after the game has drawn and before it blocks. It runs at most
  once per 2 BIOS ticks and only while the BIOS reports mode 13h, so the VBE intro is skipped.
  It compares `A000h` with a 64,000-byte shadow, emits one 1-line `rect` per changed row, updates
  the shadow, and closes the frame with `frame_end`. An unchanged screen emits nothing.
- **Palette**: hooked at the three DAC writers — `fn_WriteDAC` (partial updates, `DS:SI`),
  `sub_SetDACBlock` (BIOS `1017h`, `ES:DX`), `sub_WriteDAC768` (the intro loader, already 6-bit).
- **State**: one DOS block (65,024 bytes) allocated at start-up:
  `0000h..F9FFh` shadow, `FA00h..FD1Fh` message buffer, `FD20h..` handle/sequence/tick/row.

Not yet: input, audio, session headers, keyframes, VBE frames, palette deduplication, compression.
A fade still writes a full 256-entry palette message per step, so recordings are larger than they
need to be.

## Implementation gotchas

- **Segment alignment is load-bearing.** The hooks add 30 bytes of code before `S1DEA`, which
  would move the data segment's paragraph prefix from 7 to 5. The game uses hard-coded DS offsets
  (for example the local `EMMXXXX0` name at `0CA0h`), so the shift breaks its EMS init (it aborts
  with "Minimum of 1MB Expanded RAM must be available."). A 2-byte pad at the end of `S1DC9`
  restores prefix 7. `build-stream.sh` verifies both the prefix (S1DEA data starts at
  `mod 16 == 7`) and the `EMMXXXX0` delta (`0C99h`) and refuses to stage a drifted build.
- The palette hook must read `DS:SI`, not `CS:SI`: `fn_WriteDAC` uses `lodsb`, and its callers
  only `push cs` so that its `retf` returns to them.
- The block is allocated before `fn_07010`/`fn_01BE8`; it needs ~64 KB of conventional memory.
  If allocation or file creation fails, `v_StreamSeg` stays 0 and every hook is a no-op.

## Validation (2026-10-19)

No-input 40 s run, default oracle profile:

- 846 records: 1 `set_mode`, 447 `palette`, 396 `rect`, 2 `frame_end`; 479,755 bytes;
- the client's SW Games logo frame matches the emulator's 15 s snapshot (same logo, same colours);
- oracle build re-verified after the batch: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.

Limitations: timing-dependent (the run length bounds what is captured), mode 13h only, and the
instrumented build is slower than the original.
