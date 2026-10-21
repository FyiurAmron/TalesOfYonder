# Oracle harness (DOSBox-X)

Run the original `SWREG.EXE` under DOSBox-X headlessly, capture evidence (screen
snapshots, save states, logs) and keep the save states as the reference for
differential testing. Configs and scripts live in `tools/dosbox/`; every run writes to
`build/runs/oracle/<timestamp>/`.

## Running

    sh tools/dosbox/baseline.sh [seconds]      # default 75

Requirements: `Xvfb`, `dosbox-x` (2026.01.02 tested), `python3` (Pillow;
libX11/libXtst at runtime).

The run boots the original distribution from `_ref/orig-bin/` (md5-checked against
`engine/asm/swreg/tools/SWREG.EXE.md5` inside the run) via `CH2.BAT` on a private Xvfb display,
with no input.

## Artifacts (per run)

| Path | What |
|---|---|
| `capture/screen_0{15,35,55}.png` | X window snapshots (title screen, intro) |
| `saves/{1..4}.sav` | DOSBox-X save states (autosave, 15 s interval, rotating slots) |
| `run.log`, `stdout.log` | DOSBox-X logs |
| `oracle-md5.txt` | reference identity check (`SWREG.EXE: OK`) |
| `game/` | the run's copy of the original distribution |

## Pieces

- `tools/dosbox/dosbox-x.conf` — oracle profile: svga_s3 (VBE 1.2), EMS/XMS/UMB, SB16+OPL3,
  16 MB, fixed 30 000 cycles, autosave on, no input macros.
- `tools/dosbox/baseline.sh` — Xvfb + DOSBox-X + snapshots + artifact collection.
- `tools/dosbox/snap.py` — X window snapshots (`XGetImage` via ctypes, PNG via Pillow).
- `tools/dosbox/keys.py` — XTEST key injection (hotkeys like `F12+P`, `Ctrl+F9`; later:
  scripted gameplay input).
- `tools/dosbox/audio.sh` — same run with DOSBox-X audio/OPL capture (below).
- `tools/stream/wav.py` — WAV capture statistics and silence check.
- `tools/stream/dro.py` — DRO 2.0 (OPL register log) decoder: `summary`, `dump`.

## Audio and OPL capture (`audio.sh`)

    AUDIO_MODE=both sh tools/dosbox/audio.sh [seconds]      # wav | dro | both, default 45

Runs the original `CH2.BAT` with DOSBox-X's `DX-CAPTURE` wrapping the game
executable, so everything the game and SBFMDRV send to the FM chip is recorded:

    DX-CAPTURE /A /O SWREG.EXE %1 %2 %3 %4 /W /B     (patched copy of CH2.BAT)

Two details matter: the capture has to start inside the batch environment
(wrapping `CH2.BAT` itself stops the capture at the driver's TSR install), and
the game refuses to run from a bare prompt (`Please run Yendorian Tales Chapter 2
from CH2.BAT`) because it requires the `/B` switch the batch passes.

| Path | What |
|---|---|
| `capture/swreg_000.wav` | mixed audio, 48 kHz stereo (`tools/stream/wav.py`) |
| `capture/swreg_000.dro` | OPL register writes, DRO 2.0 (`tools/stream/dro.py`) |

`audio.sh` also checks that every note-on gap in the register log lies on the
tick grid of the shipped blocks (96 Hz = 10.42 ms), which is what proves the game
drives the resident music engine on its own.  Observed (40 s run):

    WAV swreg_000.wav: 39.97 s, 2 ch 48000 Hz, peak 41.8% fs, rms 1919, 79/79 windows with signal
    swreg_000.dro: DRO 2.0, hardware 0, 1903 pairs (1379 writes, 489 short / 35 long delays)
      101 distinct registers, 267 note-on writes
    DRO grid: 267 note-ons, 266/266 gaps on the 10.42 ms tick grid (tolerance 4 ms)

The register log is the port's OPL reference: the driver harness already
compares a controlled playback run against the static walk of a music block
(`sh engine/asm/sbfmdrv/test/run.sh music` with `MUSIC_PLAY=1 MUSIC_CAP=both`, checked by
`engine/asm/sbfmdrv/test/music.py check --dro`).

### DRO files

DRO 2.0 is "DBRAWOPL", version, pair count, millisecond length, hardware type,
format, compression, short/long delay codes, codemap length + table, then pairs
of (codemap index, value).  Delays are explicit codes: a short delay is
`value + 1` ms, a long delay `(value + 1) * 256` ms; every other index maps
through the codemap, with bit 7 selecting the second register bank.  DOSBox-X
dumps a snapshot of the current register state first, so the opening writes are
SBFMDRV's initialization rather than music; a single `BDh` write at that point
means melodic mode (the game never selects percussion).

## Known quirks

- DOSBox-X 2026.01.02's built-in screenshot (`F12+P`) aborts the process on the
  game's VESA mode (libpng misuse); text-mode screenshots are fine. The harness
  grabs the X window instead.
- That X-window grab also goes black while the game is in VESA mode (observed with the
  stream build during the VBE intro): use it for text and mode-13h screens only. For
  VBE content, compare against a recorded stream (`tools/stream/replay.py`) instead.
- On this host Xvfb falls back to abstract sockets; the `/tmp/.X11-unix` "failed to
  bind listener" lines in `xvfb.log` are benign.
- Save states are tied to the DOSBox-X version and machine type (see the `[dosbox]`
  `forceloadstate` option if a mismatch ever needs forcing).
