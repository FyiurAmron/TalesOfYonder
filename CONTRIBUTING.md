# Contributing

This repository is a recreation of *Yendorian Tales Book I, Chapter 2* (`SWREG.EXE`, 1996,
SW Games / Spectrum Pacific Publishing) for modern systems. Two documents define the project —
read them in order:

- **`docs/porting.md`** — the plan: assets, strategy, stages, milestones, risks, progress log.
- **`docs/protocol.md`** — the companion protocol (engine ↔ client/host), v0 draft.

## Repository layout

| Path | What |
|---|---|
| `docs/` | project docs in lowercase kebab-case: `porting.md` (plan), `protocol.md` (companion protocol), `disassembly.md` (working SWREG disassembly), `oracle.md` (DOSBox-X harness), `asm-style.md` (annotation rules), `layout.md` (this tree) |
| `AGENTS.md`, `CONTRIBUTING.md` | agent rules and this file — the only root-level markdown |
| `_ref/` | **read-only reference** — original distribution, the frozen disassembly generation pipeline (incl. the historic MASM source), and the vendored mining project. Copy from it, never edit it. |
| `engine/asm/swreg/` | the working `SWREG.EXE` disassembly: source in `engine/asm/swreg/src/`, build/verify machinery in `engine/asm/swreg/tools/` (outputs land in `build/swreg/oracle/`) |
| `engine/asm/sbfmdrv/` | the byte-perfect `SBFMDRV.COM` disassembly (`src/`, `tools/`) and its DOSBox-X black-box harness (`test/`) |
| `engine/c/` | (planned) the C core and platform layer |
| `client/` | the C# companion client (SDL3), `client/server.sh` for the live link and `client/publish-win.sh` for the Windows bundle |
| `tools/dosbox/` | DOSBox-X profiles and headless runners: oracle baseline (`baseline.sh`), audio/OPL capture (`audio.sh`), X11 helpers (`keys.py`, `snap.py`), the client link profile (`dosbox-x-client.conf`); outputs under `build/runs/` |
| `tools/stream/` | recording tooling: `replay.py` (the reference decoder), `compare_streams.py`, `dro.py`, `wav.py` |
| `tests/` | gates: `replay/run.sh` (offline), `audio/run.sh` (synth), `link/run.sh` (live), `data/` fixtures |
| `build/` | **all** build outputs and scratch (gitignored); scratch goes to `build/tmp/` |

## Ground rules

1. **`_ref/` is read-only.** Copy what you need out of it into a working project (e.g.
   `engine/asm/swreg/`); never edit it. A pre-commit hook (`.githooks/`) rejects any commit that changes
   `_ref/` — enable it once per clone with `git config core.hooksPath .githooks`. The resource
   viewer/editor unlocks only after the main port is finished.
2. **Byte-exactness is the invariant for disassembly work.** Any edit under `engine/asm/swreg/`
   must leave the rebuild identical to the reference (`SWREG.EXE` MD5
   `c3bdd3054e55a1e0b57c9780654dc982`, SHA-256 `3671ce1d…`).
3. **No build artifacts in git.** If a tool produced it, it belongs under `build/`.
4. **Small, focused commits**, Conventional Commits style:
   `<type>(<scope>): <summary>` — `feat`, `fix`, `docs`, `test`, `build`, `chore`, `refactor`;
   e.g. `docs(disasm): annotate the EMS manager`, `test: add the oracle harness`.
   Keep a feature's code and docs in the same commit; squash later fixes into the commit they fix.
   Day-to-day work happens on `dev`; `main` is the published branch and is updated only when the
   maintainer asks.
5. **Don't rewrite published history** (amend/rebase/force-push) unless explicitly agreed. On
   local `dev`, the latest commit may be amended/squashed; any older commit needs agreement.
6. **Docs live in `docs/`** as lowercase kebab-case files (`docs/porting.md`, `docs/protocol.md`);
   only `README.md`, `AGENTS.md` and `CONTRIBUTING.md` stay at the root.

## Working on the disassembly (byte-exact asm)

`engine/asm/swreg/src/swreg.asm` is the *hand-maintained* source (copied from the frozen baseline in
`_ref/disasm/uasm/swreg.asm`). Rename labels, add comments/EQU constants/macros, group modules
into subsystem banners — but the bytes must not move.

The gate, after every batch (~1 min of assembly):

    sh engine/asm/swreg/tools/verify.sh

Expected tail: `SWREG.EXE: OK` and `IDENTICAL`. On mismatch, `engine/asm/swreg/tools/verify_asm.py`
reports the exact instruction(s) with expected vs got bytes.

Notes:
- MASM is historic only: the frozen MASM source (`_ref/disasm/masm/`) is reference-only; UASM is the build path.
- UASM ≥ 2.53 is required (2.57 tested). Manual assembler invocations should target `build/tmp/`.
- Annotation style: `docs/asm-style.md` (semantic names, routine headers, `; SEAM` tags, the
  verify gate). `loc_XXXXX` labels stay; macros only where they expand to the exact proven
  encodings; batches stay per module.

### The seam split and its pin

All integration code lives in `engine/asm/swreg/src/seam.asm`; the game source only carries tagged hook sites
and the `include`, both inside `IFDEF SEAM`. `verify.sh` first checks
`python3 engine/asm/swreg/tools/seam_pin.py --check` — a hash of `swreg.asm` with the seam blocks stripped,
pinned in `engine/asm/swreg/src/swreg.pin`. **An edit to game code that dodges the byte gate but changes the
stripped text fails here** (re-pin only after a deliberate change): `seam_pin.py --update`.

Seam builds (not byte-identical, on purpose): `engine/asm/swreg/tools/build.sh --variant file`
(the product link), `--variant serial` (the obsolete COM1 link), `--variant tee` (both),
`--variant log` (file + `SEAMLOG.TXT`). Each runs `fixhdr.py` and `check_mz_layout.py` (S1DEA
paragraph prefix 7, `EMMXXXX0` delta `0C99h`) and stages the minimum game files. See
`docs/stream.md` and `docs/client.md`.
- Keep batches verifiable — if a batch takes much longer than a minute to check, split it.

## Working on the tooling (Python)

`engine/asm/swreg/tools/verify_asm.py` verifies a rebuilt candidate against the reference, per instruction.

- Python 3 + `capstone` (5.x). The verifier reads the reference via `$SWREG_EXE` (default:
  `_ref/orig-bin/SWREG.EXE`): `python3 engine/asm/swreg/tools/verify_asm.py [candidate.exe]`
- The generation pipeline (`build.py`, `emit.py`, `identify/`, …) stays read-only in
  `_ref/disasm/tools/` — it is the provenance of the frozen baseline; don't edit it.
- Follow the existing style: stdlib + capstone, compact, no third-party dependencies.

## Working on the C port / C# front-end (planned)

Not started yet; the rules that will apply are in `docs/porting.md` §3 and `docs/protocol.md`:

- faithful first: exact 16-bit semantics, no UB, explicit widths;
- platform access only through the service layer; the client/host boundary is `docs/protocol.md`;
- C stays in a subset that transliterates cleanly to C#;
- every module lands with tests before it is refactored.

The live client (`client/`, `docs/client.md`) is a **development harness for the boundary**, not the
M8/M9 deliverable: it decodes VIDEO v0, renders, and gives the seam something to talk to.
`client/publish-win.sh` packages it as a self-contained Windows bundle for trials.
The M8/M9 rules above still govern the product client.

## Testing & gates

| Area | Gate before commit |
|---|---|
| `engine/asm/swreg/` | `verify.sh` ends `OK` + `IDENTICAL` (includes the `swreg.pin` check); no commit ever changes `_ref/` |
| seam builds | `build.sh --variant serial|file|tee|log` assemble with 0 warnings and pass `check_mz_layout.py` |
| `tools/dosbox/` | `sh tools/dosbox/baseline.sh` completes: oracle md5 `OK`, snapshots + save states present |
| `client/` | `dotnet build client -c Release` clean; `sh tests/replay/run.sh` renders byte-identically to `tools/stream/replay.py`; `sh tests/audio/run.sh` renders the DRO fixture and opens SDL audio; `sh tests/link/run.sh` (obsolete serial link, optional) reports every received message byte-exact |
| Windows bundle | `publish-win.sh` -> `yt2-win-x64.zip`; target smoke test |
| docs | paths/links plausible; `docs/porting.md` progress log updated for milestones |
| C / C# (future) | unit tests + replay/checkpoint harness green |

## Environment setup

- **UASM**: <https://github.com/Terraspace/UASM/releases> — ≥ 2.53 (2.57 tested); put the binary
  on `PATH` (e.g. `~/.local/bin/uasm`).
- **Python 3** with `capstone`.
- **DOSBox-X** for oracle runs (outputs under `build/`).
- **GitHub CLI** (`gh`) for repository operations.

## Assets & provenance

`_ref/orig-bin/` contains the original 1996 distribution, kept for preservation and reverse
engineering. It is reference data only.
