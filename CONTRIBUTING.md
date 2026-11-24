# Contributing

This repository is a recreation of *Yendorian Tales Book I, Chapter 2*
(`SWREG.EXE`, 1996, SW Games / Spectrum Pacific Publishing) for modern systems.
The project is defined by these documents — read them in order:

- **`docs/porting.md`** — the plan: assets, strategy, stages, verification,
  risks.
- **`docs/roadmap.md`** — milestones, open items and the next steps.
- **`docs/progress.md`** — the dated progress log.
- **`docs/protocol.md`** — the companion protocol (engine ↔ client/host), v0
  draft.

## Repository layout

| Path | What |
|---|---|
| `docs/` | project docs in lowercase kebab-case: `porting.md` (plan), `roadmap.md` (milestones + next), `progress.md` (dated log), `protocol.md` (companion protocol), `disassembly.md` (working SWREG disassembly), `oracle.md` (DOSBox-X harness), `asm-style.md` (annotation rules), `layout.md` (this tree) |
| `AGENTS.md`, `CONTRIBUTING.md` | agent rules and this file — the only root-level markdown |
| `_ref/` | **read-only reference** — original distribution, the frozen disassembly generation pipeline (incl. the historic MASM source), and the vendored mining project. Copy from it, never edit it. |
| `engine/asm/swreg/` | the working `SWREG.EXE` disassembly: source in `engine/asm/swreg/src/`, build/verify machinery in `engine/asm/swreg/tools/` (outputs land in `build/swreg/oracle/`) |
| `engine/asm/sbfmdrv/` | the byte-perfect `SBFMDRV.COM` disassembly (`src/`, `tools/`) and its DOSBox-X black-box harness (`test/`) |
| `engine/c/` | the B1 pseudo-port/executable spec and platform service layer: `build.sh`/`build.bat`, `yt2stub`, unit tests under `tests/`; B2 replaces it as the product |
| `client/` | the C# companion client (SDL3) and `client/server.sh` for the obsolete serial link |
| `tools/publish-*.sh` | platform bundle publishers (client + selected server) |
| `tools/dosbox/` | DOSBox-X profiles and headless runners: oracle baseline (`baseline.sh`), audio/OPL capture (`audio.sh`), X11 helpers (`keys.py`, `snap.py`), the client link profile (`dosbox-x-client.conf`); outputs under `build/runs/` |
| `tools/stream/` | recording tooling: `replay.py` (the reference decoder), `compare_streams.py`, `dro.py`, `wav.py` |
| `tests/` | gates: `replay/run.sh` (offline), `audio/run.sh` (synth), `link/run.sh` (live), `data/` fixtures |
| `build/` | **all** build outputs and scratch (gitignored); scratch goes to `build/tmp/` |

## Ground rules

1. **`_ref/` is read-only.** Copy what you need out of it into a working project
   (e.g. `engine/asm/swreg/`); never edit it. A pre-commit hook (`.githooks/`)
   rejects any commit that changes `_ref/` — enable it once per clone with `git
   config core.hooksPath .githooks`. The resource viewer/editor unlocks only
   after the main port is finished.
2. **Byte-exactness is the invariant for disassembly work.** Any edit under
   `engine/asm/swreg/` must leave the rebuild identical to the reference
   (`SWREG.EXE` MD5 `c3bdd3054e55a1e0b57c9780654dc982`, SHA-256 `3671ce1d…`).
3. **No build artifacts in git.** If a tool produced it, it belongs under
   `build/`.
4. **Small, focused commits**, Conventional Commits style: `<type>(<scope>):
   <summary>` — `feat`, `fix`, `docs`, `test`, `build`, `chore`, `refactor`;
   e.g. `docs(disasm): annotate the EMS manager`, `test: add the oracle
   harness`. Keep a feature's code and docs in the same commit; squash later
   fixes into the commit they fix. Day-to-day work happens on `dev`; `main` is
   the published branch and is updated only when the maintainer asks.
5. **Don't rewrite published history** (amend/rebase/force-push) unless
   explicitly agreed. On local `dev`, the latest commit may be amended/squashed;
   any older commit needs agreement.
6. **Docs live in `docs/`** as lowercase kebab-case files (`docs/porting.md`,
   `docs/roadmap.md`, `docs/progress.md`, `docs/protocol.md`); only `README.md`,
   `AGENTS.md` and `CONTRIBUTING.md` stay at the root.

## Working on the disassembly (byte-exact asm)

`engine/asm/swreg/src/swreg.asm` is the *hand-maintained* source (copied from
the frozen baseline in `_ref/disasm/uasm/swreg.asm`). Rename labels, add
comments/EQU constants/macros, group modules into subsystem banners — but the
bytes must not move.

The gate, after every batch (~1 min of assembly):

    sh engine/asm/swreg/tools/verify.sh

Expected tail: `SWREG.EXE: OK` and `IDENTICAL`. On mismatch,
`engine/asm/swreg/tools/verify_asm.py` reports the exact instruction(s) with
expected vs got bytes.

Notes:
- MASM is historic only: the frozen MASM source (`_ref/disasm/masm/`) is
  reference-only; UASM is the build path.
- UASM ≥ 2.53 is required (2.57 tested). Manual assembler invocations should
  target `build/tmp/`.
- Annotation style: `docs/asm-style.md` (semantic names, routine headers, `;
  SEAM` tags, the verify gate). `loc_XXXXX` labels stay; macros only where they
  expand to the exact proven encodings; batches stay per module.

### The seam split and its pin

All integration code lives in `engine/asm/swreg/src/seam.asm`; the game source
only carries tagged hook sites and the `include`, both inside `IFDEF SEAM`.
`verify.sh` first checks `python3 engine/asm/swreg/tools/seam_pin.py --check` —
a hash of `swreg.asm` with the seam blocks stripped, pinned in
`engine/asm/swreg/src/swreg.pin`. **An edit to game code that dodges the byte
gate but changes the stripped text fails here** (re-pin only after a deliberate
change): `seam_pin.py --update`.

Seam builds (not byte-identical, on purpose): `engine/asm/swreg/tools/build.sh
--variant file` (the product link), `--variant serial` (the obsolete COM1 link),
`--variant tee` (both), `--variant log` (file + `SEAMLOG.TXT`). Each runs
`fixhdr.py` and `check_mz_layout.py` (S1DEA paragraph prefix 7, `EMMXXXX0` delta
`0C99h`) and stages the minimum game files. See `docs/stream.md` and
`docs/client.md`.
- Keep batches verifiable — if a batch takes much longer than a minute to check,
  split it.

## Working on the tooling (Python)

`engine/asm/swreg/tools/verify_asm.py` verifies a rebuilt candidate against the
reference, per instruction.

- Python 3 + `capstone` (5.x). The verifier reads the reference via `$SWREG_EXE`
  (default: `_ref/orig-bin/YT2/SWREG.EXE`): `python3
  engine/asm/swreg/tools/verify_asm.py [candidate.exe]`
- The generation pipeline (`build.py`, `emit.py`, `identify/`, …) stays
  read-only in `_ref/disasm/tools/` — it is the provenance of the frozen
  baseline; don't edit it.
- Follow the existing style: stdlib + capstone, compact, no third-party
  dependencies.

## Working on the C port / C# front-end

Stage B has two separate products; do not mix them. The rules are in
`docs/porting.md` §3 and `docs/protocol.md`.

- **B1, the pseudo-port in `engine/c/` today**, is the executable spec/oracle:
  emitted instruction-level C over a 16-bit guest model with guest registers,
  flat memory and a segment:offset dispatcher. It is a milestone, not the
  deliverable. Keep it working as an oracle; do not add gameplay features to it.
- **B2, the C deliverable (M8)**, is hand-written, minimal, idiomatic C: typed
  game state and normal abstractions. It has no asm, no generated instruction
  stream, no guest CPU/registers/flags, no flat `mem[]`, no segment:offset
  dispatch and no DOS/BIOS/EMS emulation in the game logic. Platform access goes
  through small service interfaces only.
- B2 rules: explicit widths where the on-disk format demands them, no UB, normal
  C control flow, and a style that translates cleanly to C#.
- Every B2 module lands with differential tests against B1 and the DOS oracle
  before the next slice starts; B1 is never linked into B2.

`engine/c/tools/build.sh` writes the Linux binaries under `build/c/`;
`engine/c/tools/build-mingw.sh` (wrapped by `build.bat`) cross-builds the same
programs for Windows x64 into `build/c-win/` with MinGW-w64. MSVC is not part of
the supported build. `tests/c/windows.sh` checks the PE32+ images and runs them
under Wine, requiring byte-identical logs and dumps to the Linux build; it skips
cleanly when the toolchain is absent. The committed B1 gate is `sh
tests/c/run.sh`: it runs the unit tests (including the typed save model),
re-emits the asm fixture byte-for-byte through the C folder backend, renders the
stub output with the C# client and the Python decoder, and drives the folder
input (keyframe, key, quit, live append), stream and null backends. `yt2stub`
remains a deterministic harness for the boundary itself (fixture replay,
folder/stream/null inputs, keyframe and quit paths); the B1 game is `yt2game`,
built from the emitted transliterated routines plus the hand-written
DOS/BIOS/EMS services. The B1 file/save record model is
`engine/c/include/yt2/data.h` (see `docs/structures.md`); the wire spec is
frozen v0 in `docs/protocol.md`. Neither is the B2 game-state model.

The live client (`client/`, `docs/client.md`) is a **development harness for the
boundary**, not the M9/M10 deliverable: it decodes VIDEO v0, renders, and gives
the seam something to talk to. `tools/publish-win.sh` packages the client and
selected server as a self-contained Windows bundle for trials. `--server asm`
stages the seam build; `--server c` stages the cross-built B1 `yt2game.exe` from
`build/c-win` (no DOSBox-X); both are development bundles until B2/M9. The
M9/M10 rules above still govern the product client.

## Testing & gates

| Area | Gate before commit |
|---|---|
| `engine/asm/swreg/` | `verify.sh` ends `OK` + `IDENTICAL` (includes the `swreg.pin` check); no commit ever changes `_ref/` |
| seam builds | `build.sh --variant serial|file|tee|log` assemble with 0 warnings and pass `check_mz_layout.py` |
| `tools/dosbox/` | `sh tools/dosbox/baseline.sh` completes: oracle md5 `OK`, snapshots + save states present |
| `client/` | build, replay, timed session, audio and optional serial-link gates (see `tests/README.md`) |
| `engine/c/` | `sh tests/c/run.sh`: unit tests, asm-fixture parity, client/Python render match, scripted/live input, stream + null backends; `tests/c/windows.sh` when MinGW-w64 and Wine are present |
| Windows bundle | `tools/publish-win.sh --server asm` or `--server c` -> `yt2-win-x64.zip`; target smoke test |
| docs | paths/links plausible; `docs/progress.md` updated for milestones and `docs/roadmap.md` next steps current |
| C / C# (future) | unit tests + replay/checkpoint harness green |

## Environment setup

- **UASM**: <https://github.com/Terraspace/UASM/releases> — ≥ 2.53 (2.57
  tested); put the binary on `PATH` (e.g. `~/.local/bin/uasm`).
- **Python 3** with `capstone`.
- **DOSBox-X** for oracle runs (outputs under `build/`).
- **GitHub CLI** (`gh`) for repository operations.

## Assets & provenance

`_ref/orig-bin/YT2/` contains the original 1996 distribution, kept for
preservation and reverse engineering. It is reference data only.
