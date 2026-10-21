# YT2 — Yendorian Tales Book I, Chapter 2, recreated

A staged recreation of the 1996 DOS game `SWREG.EXE` (SW Games / Spectrum Pacific Publishing) for
modern systems. The rule is byte-exactness: the recreation starts from a disassembly that rebuilds
the original executable bit for bit, then grows a C core and finally a C# front-end on top of a
documented engine ↔ client protocol (`docs/protocol.md`).

**Status.** M1 is done: the disassembly is annotated, byte-exact and pinned, and the integration
seam (`engine/asm/swreg/src/seam.asm`) streams the live display over a serial link and takes
keyboard input back. The C# client (`client/`) mirrors the screen in a window and forwards keys,
and the Windows bundle is one script away. The C stage
(`engine/c/`) has not started. `docs/porting.md` §10 is the live progress log.

## Quick start

    sh engine/asm/swreg/tools/verify.sh      # assemble + byte gate: SWREG.EXE: OK, IDENTICAL
    sh tools/dosbox/baseline.sh 75           # headless oracle run of the original (_ref/orig-bin)

The live link (two terminals, one X display):

    sh engine/asm/swreg/tools/build.sh --variant serial   # or let server.sh do it
    sh client/server.sh                                   # game window, COM1 -> TCP :2324
    dotnet run --project client -c Release                # the companion client

Recordings and gates:

    sh tools/dosbox/stream.sh 45             # file backend, run + replay the capture to PNG
    sh tests/replay/run.sh                   # client render == Python render, byte for byte
    sh tests/link/run.sh                     # live engine -> DOSBox-X -> client link check

Windows bundle (self-contained, no .NET install on the target):

    sh client/publish-win.sh                 # -> build/dist/yt2-win-x64.zip

## Layout

| Path | What |
|---|---|
| `engine/asm/swreg/` | the byte-perfect `SWREG.EXE` disassembly and its build/verify tools |
| `engine/asm/sbfmdrv/` | the byte-perfect `SBFMDRV.COM` (FM driver) disassembly and its black-box harness |
| `engine/c/` | (planned) the C core and platform layer |
| `client/` | the C# companion client (SDL3) and the Windows packaging |
| `tools/` | host tooling: `dosbox/` profiles and runners, `stream/` recording tools, `repro/` wire reproductions |
| `tests/` | committed gates (`replay`, `link`) and fixtures |
| `docs/` | plan, protocol, disassembly/client/stream notes |
| `_ref/` | read-only reference: the original distribution, the frozen disassembly, the mining project |
| `build/` | every output (gitignored): `swreg/<variant>`, `sbfmdrv/`, `client/`, `runs/<kind>/<stamp>`, `dist/`, `tmp/` |

`docs/layout.md` has the full tree and the rules that keep it in that shape.

## Docs

- `docs/porting.md` — the plan: assets, strategy, milestones, risks, progress log (§10).
- `docs/protocol.md` — the engine ↔ client protocol (v0).
- `docs/disassembly.md` — building and verifying the asm; source conventions.
- `docs/client.md` — the C# client, the seam link, the Windows bundle.
- `docs/stream.md` — the seam builds and the recording format.
- `docs/oracle.md` — the DOSBox-X oracle harness.
- `docs/sbfmdrv.md` — the FM driver disassembly.
- `CONTRIBUTING.md` — the rules; read this before changing anything.
- `AGENTS.md` — operational notes for AI agents.

## Requirements

`uasm` ≥ 2.53 (2.57 tested), Python 3 with `capstone` (and `Pillow` for snapshots and replays),
DOSBox-X, the .NET SDK, and `Xvfb` for headless runs. Details in `CONTRIBUTING.md` §Environment setup.

## Provenance

`_ref/orig-bin/` holds the original 1996 distribution, kept for preservation and reverse engineering;
it is reference data and is never modified. The game itself is © 1996 SW Games / Spectrum Pacific
Publishing.
