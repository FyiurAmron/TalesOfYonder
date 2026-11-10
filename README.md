# YT2 — Yendorian Tales Book I, Chapter 2, recreated

A staged recreation of the 1996 DOS game `SWREG.EXE` (SW Games / Spectrum Pacific Publishing) for
modern systems. The rule is byte-exactness: the recreation starts from a disassembly that rebuilds
the original executable bit for bit, then passes through three deliberate stages — a quick
pseudo-port as the executable spec, a real hand-written C port, and finally the forward-looking
C# port — on top of a documented engine ↔ client protocol (`docs/protocol.md`).

**Status.** M7, the B1 pseudo-port, is done: the disassembly is annotated, byte-exact and pinned;
the C11 tree in `engine/c/` is a 100% C transcription of the annotated asm (instruction-level
emitted C over a 16-bit guest model, with a segment:offset dispatcher) and runs the original to
the title, through the scripted M5 gameplay slice, the save/load panel and the sound path. It
speaks the frozen v0 protocol over the shared folder link (`OUTPUT.BIN`/`INPUT.BIN`), builds as
64-bit Linux binaries and Windows x64 PE32+ images, and is the differential oracle. **It is a
milestone, not the C deliverable.** The next milestone, M8/B2, is the real C port: a minimal,
hand-written, idiomatic C engine with typed game state and no asm, guest CPU/memory or emulation
glue in the product path. The C# port (M10) starts from B2. The C# client (`client/`) remains the
SDL3 development host; `docs/progress.md` is the live log and `docs/roadmap.md` has the stages.

## Quick start

    sh engine/asm/swreg/tools/verify.sh      # assemble + byte gate: SWREG.EXE: OK, IDENTICAL
    sh tools/dosbox/baseline.sh 75           # headless oracle run of the original (_ref/orig-bin)

The live link (folder transport):

    sh engine/asm/swreg/tools/build.sh --variant file  # -> build/swreg/file/ (exe + game files)
    sh tools/publish-linux.sh --server asm             # client + asm server -> build/dist/yt2-linux-x64/
    sh build/dist/yt2-linux-x64/play.sh                # DOSBox-X and the client, one command

Without the bundle (same engine build, client from the tree):

    dotnet run --project client -c Release -- \
        --follow build/swreg/file/OUTPUT.BIN --input build/swreg/file/INPUT.BIN

The old COM1/nullmodem link (`--variant serial`, `client/server.sh`, `--tcp`) is obsolete and kept
for fun; `docs/client.md` has both.

Recordings and gates:

    sh tools/dosbox/stream.sh 45             # file backend, run + replay the capture to PNG
    sh tests/c/run.sh                        # B1 pseudo-port gate: unit, asm parity, client render, input backends
    sh engine/c/tools/build-mingw.sh         # Windows x64 PE32+ cross-build -> build/c-win/
    sh tests/c/windows.sh                    # PE32+ check + Wine unit/parity runs (skips without mingw/wine)
    sh tests/replay/run.sh                   # client render == Python render, byte for byte
    sh tests/session/run.sh                  # timed BIN / YT2S capture, playback and input replay
    sh tests/link/run.sh                     # obsolete serial link check, kept for fun

Windows bundle (self-contained, no .NET install on the target):

    sh tools/publish-win.sh --server asm     # DOSBox-X seam server -> build/dist/yt2-win-x64.zip
    sh tools/publish-win.sh --server c       # native C engine server (build/c-win)

## Layout

| Path | What |
|---|---|
| `engine/asm/swreg/` | the byte-perfect `SWREG.EXE` disassembly and its build/verify tools |
| `engine/asm/sbfmdrv/` | the byte-perfect `SBFMDRV.COM` (FM driver) disassembly and its black-box harness |
| `engine/c/` | the B1 pseudo-port (executable spec/oracle): emitted instruction-level C, platform service layer, `yt2game`/`yt2stub`/`yt2saveinfo`, Linux/Windows x64 builds; the B2 real C port replaces it as the product |
| `client/` | the C# companion client (SDL3) and platform bundle files |
| `tools/` | host tooling, including the `publish-*.sh` bundle publishers |
| `tests/` | committed gates (the list is in `tests/README.md`) and fixtures |
| `docs/` | plan, roadmap, progress log, protocol, disassembly/client/stream notes |
| `_ref/` | read-only reference: the original distribution, the frozen disassembly, the mining project |
| `build/` | every output (gitignored): `swreg/<variant>`, `sbfmdrv/`, `client/`, `runs/<kind>/<stamp>`, `dist/`, `tmp/` |

`docs/layout.md` has the full tree and the rules that keep it in that shape.

## Docs

- `docs/porting.md` — the plan: assets, strategy, stages, verification, risks.
- `docs/roadmap.md` — milestones, open items and the next steps.
- `docs/progress.md` — the dated progress log.
- `docs/protocol.md` — the engine ↔ client protocol (frozen v0).
- `docs/structures.md` — the B1 typed save/party/data records and their offsets.
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
