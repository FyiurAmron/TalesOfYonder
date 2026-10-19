# Oracle harness (DOSBox-X)

Run the original `SWREG.EXE` under DOSBox-X headlessly, capture evidence (screen
snapshots, save states, logs) and keep the save states as the reference for
differential testing. Configs and scripts live in `harness/`; every run writes to
`build/oracle/<timestamp>/`.

## Running

    sh harness/baseline.sh [seconds]      # default 75

Requirements: `Xvfb`, `dosbox-x` (2026.01.02 tested), `python3` (Pillow;
libX11/libXtst at runtime).

The run boots the original distribution from `_ref/orig-bin/` (md5-checked against
`asm/tools/SWREG.EXE.md5` inside the run) via `CH2.BAT` on a private Xvfb display,
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

- `harness/dosbox-x.conf` — oracle profile: svga_s3 (VBE 1.2), EMS/XMS/UMB, SB16+OPL3,
  16 MB, fixed 30 000 cycles, autosave on, no input macros.
- `harness/baseline.sh` — Xvfb + DOSBox-X + snapshots + artifact collection.
- `harness/snap.py` — X window snapshots (`XGetImage` via ctypes, PNG via Pillow).
- `harness/keys.py` — XTEST key injection (hotkeys like `F12+P`, `Ctrl+F9`; later:
  scripted gameplay input).

## Known quirks

- DOSBox-X 2026.01.02's built-in screenshot (`F12+P`) aborts the process on the
  game's VESA mode (libpng misuse); text-mode screenshots are fine. The harness
  grabs the X window instead.
- On this host Xvfb falls back to abstract sockets; the `/tmp/.X11-unix` "failed to
  bind listener" lines in `xvfb.log` are benign.
- Save states are tied to the DOSBox-X version and machine type (see the `[dosbox]`
  `forceloadstate` option if a mismatch ever needs forcing).
