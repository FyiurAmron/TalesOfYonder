# SBFMDRV.COM disassembly

Byte-perfect UASM source for the Creative Sound Blaster FM driver shipped with
Yendorian Tales Book I, Chapter 2:

    _ref/orig-bin/SBFMDRV.COM
    size 7211 bytes, MD5 0f679017d543b0947fc5a33456ba38a7

The driver is a COM TSR (version 1.34).  It installs a resident INT API on a
free vector in 80h..BFh, hooks INT 08h and INT 09h, and runs the FM/OPL music
engine.  The game locates it by the "FMDRV" signature at resident offset 103h.

## Layout

- `src/sbfmdrv.asm` - the checked-in, annotated source.  One `_TEXT` segment,
  `org 100h`, assembled with `uasm -bin` into a raw COM image.
- `tools/generate.py` - development tool that recursively disassembles the
  reference, emits a baseline source, and assembles it until it is byte
  identical.  It also documents the handful of instructions where UASM picks a
  different (non-identical) encoding; those are emitted as `db` with the
  original mnemonic in a comment.
- `tools/build.sh` - assemble `src/sbfmdrv.asm` into `build/sbfmdrv/`.
- `tools/verify.sh` - build and byte-verify against `_ref/orig-bin/SBFMDRV.COM`.
- `tools/verify.py` - exact size/MD5/per-byte comparison.
- `tools/SBFMDRV.COM.md5` - reference hash.

## Build and verify

    sh engine/asm/sbfmdrv/tools/verify.sh

Expected output:

    SBFMDRV.COM: OK
    IDENTICAL  md5 0f679017d543b0947fc5a33456ba38a7  size 7211

The reference path can be overridden with `SBFMDRV_EXE=...` for
`tools/verify.py`.  Build outputs go to `build/sbfmdrv/` (ignored by git).

## Disassembly notes

- The generator seeds the installer, the resident INT API dispatcher (12E6h),
  the timer ISR (1226h), the INT 09h hook (12B2h), the SB IRQ stub (1826h), and
  the four dispatch tables (33Ah, 372h, 392h, 17CAh).
- Two call sites index sub-tables inside the 33Ah word array (`34Ah`, `352h`);
  those entry labels are emitted inside the table.
- The source is annotated by hand on top of the generated baseline.  Regenerating
  with `tools/generate.py` produces the unannotated baseline; the checked-in
  source is the artifact.
- No AI/co-author attribution is added to commits.

## Protocol analysis and black-box tests

- `docs/sbfmdrv.md` - installer/uninstaller, resident INT API, music engine,
  the checksum/self-patch, the OPL/DSP layer, black-box results and open items.
- `test/test.asm` - raw COM test: scans 80h..BFh for the `"FMDRV"`
  signature, patches a local `int` stub to the resident vector, exercises the
  API, and writes `RESULT.TXT`.
- `test/music.asm` - `MUSIC.COM`: plays a real `WORLD.DAT` block (`MUSIC.BLK`)
  through commands 2/4/6 and single-steps the sequencer, logging
  `S<step> P<position> X<status>` to `MUSIC.LOG`.
- `test/music.py` - extract a music record from `_ref/orig-bin/WORLD.DAT`
  (`extract`), print the static stream walk (`walk`) and compare a `MUSIC.LOG`
  against it (`check`).  `--track N` selects the id, `--song` slices exactly to
  the stop marker instead of the game's record window; `--play` validates a
  playback run, `--wav` reports a capture and `--dro` (with `--play`) compares a
  `DX-CAPTURE /O` OPL register log against the walk (key-on count and tick
  timeline).
- `test/build.sh` - assemble both tests into `build/sbfmdrv/test/`.
- `test/run.sh [api|uninstall|uninstall-late|music]` - build, run DOSBox-X headless (Xvfb,
  SB16 profile, `A220 I7 D1 H5 T6`), and print `RESULT.TXT` / `MUSIC.LOG`
  plus the cross-check (`music` mode; `MUSIC_TRACK`/`MUSIC_SONG` select the
  record, `MUSIC_PLAY=1` runs the driver's timer ISR instead of stepping,
  `MUSIC_CAP=wav|dro|both` adds a `DX-CAPTURE` audio/OPL recording,
  `MUSIC_TIMEOUT` sets the DOSBox-X time limit; `API_CAP=dro` records the API
  run's OPL writes and checks them with `test/perc.py`).
- `test/perc.py` - replay the percussion section's expected OPL writes
  (velocity table, drum registers, note words) against an `API_CAP=dro`
  capture.

The API run has two stream parts: a crafted 41-byte stream played through the
real timer path (voice allocation, notes/frequencies, self-stop) and a
deterministic single-step trace of the resident sequencer (one event per call,
positions/status/voices per line).  It shows the state machine end to end
(`ST 0 -> 1`, status `00 -> FF`, `[0DF3h] 00 -> 01`, second start `FFFEh`,
pause/resume, stop -> 0).  A second crafted stream then traces percussion mode
(`B0 67h 01h`, five drum hits and releases, `B0 67h 00h`): the `BDh` shadow
`[02F7h]`, the voice count and the rhythm flag per step, plus the drum note
slots and their frequency word.  The exact traces and their interpretation are
in `docs/sbfmdrv.md` section 6.  The uninstall run installs, runs `/U`, and the
test then reports `NO DRIVER`; `uninstall-late` loads `LATE.COM` (a TSR on `INT 60h`)
after the driver first, so `/U` must refuse with error `0005h` and leave the driver's
hooks, its API vector and that TSR in place.

The music run extracts a real record (default track 1) and checks every
sequencer position/status against the static walk of the same block; tracks 1,
8 and 18 verify with `T = N` (no foreign sequencer invocations).  The record
directory lives at `DS:0CED3h`/`DS:0CF27h` in the game's DGROUP and its
layout, plus the stale-window quirk of the shipped size table, is documented in
`docs/sbfmdrv.md` section 4.1.
