# Contributing

This repository is a recreation of *Yendorian Tales Book I, Chapter 2* (`SWREG.EXE`, 1996,
SW Games / Spectrum Pacific Publishing) for modern systems. Two documents define the project —
read them in order:

- **`docs/porting.md`** — the plan: assets, strategy, stages, milestones, risks, progress log.
- **`docs/protocol.md`** — the companion protocol (engine ↔ client/host), v0 draft.

## Repository layout

| Path | What |
|---|---|
| `docs/` | project docs in lowercase kebab-case: `porting.md` (plan), `protocol.md` (companion protocol), `disassembly.md` (working SWREG disassembly), `oracle.md` (DOSBox-X harness) |
| `AGENTS.md`, `CONTRIBUTING.md` | agent rules and this file — the only root-level markdown |
| `_ref/` | **read-only reference** — original distribution, the frozen disassembly generation pipeline (incl. the historic MASM source), and the vendored mining project. Copy from it, never edit it. |
| `asm/` | the working `SWREG.EXE` disassembly: source in `asm/src/`, build/verify machinery in `asm/tools/` (outputs land in `build/uasm/`) |
| `harness/` | oracle/test harness: DOSBox-X profile + headless runner (`baseline.sh`), X11 helpers (`keys.py`, `snap.py`); outputs under `build/oracle/` |
| `build/` | **all** build outputs and scratch (gitignored); scratch goes to `build/tmp/` |

## Ground rules

1. **`_ref/` is read-only.** Copy what you need out of it into a working project (e.g.
   `asm/`); never edit it. A pre-commit hook (`.githooks/`) rejects any commit that changes
   `_ref/` — enable it once per clone with `git config core.hooksPath .githooks`. The resource
   viewer/editor unlocks only after the main port is finished.
2. **Byte-exactness is the invariant for disassembly work.** Any edit under `asm/`
   must leave the rebuild identical to the reference (`SWREG.EXE` MD5
   `c3bdd3054e55a1e0b57c9780654dc982`, SHA-256 `3671ce1d…`).
3. **No build artifacts in git.** If a tool produced it, it belongs under `build/`.
4. **Small, focused commits on `main`**, imperative subject with a scope:
   `disasm: annotate EMS manager`, `tools: add listing parser`, `docs: …`.
5. **Don't rewrite published history** (amend/rebase/force-push) unless explicitly agreed.
6. **Docs live in `docs/`** as lowercase kebab-case files (`docs/porting.md`, `docs/protocol.md`);
   only `AGENTS.md` and `CONTRIBUTING.md` stay at the root.

## Working on the disassembly (byte-exact asm)

`asm/src/swreg.asm` is the *hand-maintained* source (copied from the frozen baseline in
`_ref/disasm/uasm/swreg.asm`). Rename labels, add comments/EQU constants/macros, group modules
into subsystem banners — but the bytes must not move.

The gate, after every batch (~1 min of assembly):

    sh asm/tools/verify.sh

Expected tail: `SWREG.EXE: OK` and `IDENTICAL`. On mismatch, `asm/tools/verify_asm.py`
reports the exact instruction(s) with expected vs got bytes.

Notes:
- MASM is historic only: the frozen MASM source (`_ref/disasm/masm/`) is reference-only; UASM is the build path.
- UASM ≥ 2.53 is required (2.57 tested). Manual assembler invocations should target `build/tmp/`.
- Annotation style is defined in `docs/asm-style.md` (created with the M1 pilot). Until then:
  semantic names (`fn_LoadWorldData`, `v_GameState`), `EQU` for struct offsets, `loc_XXXXX` stays,
  macros only where they expand to the exact proven encodings.
- Keep batches verifiable — if a batch takes much longer than a minute to check, split it.

## Working on the tooling (Python)

`asm/tools/verify_asm.py` verifies a rebuilt candidate against the reference, per instruction.

- Python 3 + `capstone` (5.x). The verifier reads the reference via `$SWREG_EXE` (default:
  `_ref/orig-bin/SWREG.EXE`): `python3 asm/tools/verify_asm.py [candidate.exe]`
- The generation pipeline (`build.py`, `emit.py`, `identify/`, …) stays read-only in
  `_ref/disasm/tools/` — it is the provenance of the frozen baseline; don't edit it.
- Follow the existing style: stdlib + capstone, compact, no third-party dependencies.

## Working on the C port / C# front-end (planned)

Not started yet; the rules that will apply are in `docs/porting.md` §3 and `docs/protocol.md`:

- faithful first: exact 16-bit semantics, no UB, explicit widths;
- platform access only through the service layer; the client/host boundary is `docs/protocol.md`;
- C stays in a subset that transliterates cleanly to C#;
- every module lands with tests before it is refactored.

## Testing & gates

| Area | Gate before commit |
|---|---|
| `asm/` | `verify.sh` ends `OK` + `IDENTICAL`; no commit ever changes `_ref/` |
| `harness/` | `sh harness/baseline.sh` completes: oracle md5 `OK`, snapshots + save states present |
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
