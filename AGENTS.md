Main rule: *If unsure, ask instead of pondering or guessing.*

`CONTRIBUTING.md` is the **source of truth** for layout, conventions, gates and history. Read it
first and follow it; if this file ever disagrees with it, `CONTRIBUTING.md` wins. This file only
adds agent-specific operational rules.

## Start of session

1. Read `CONTRIBUTING.md`, then `docs/porting.md` (plan + progress log) and `docs/protocol.md` (boundary spec).
2. Check `git status` and `git log --oneline -3`; continue on `main`, tree should be clean.
3. Before touching the disassembly, read `docs/disassembly.md`.
4. Make sure the guard hook is active: `git config core.hooksPath .githooks` (idempotent).

## Non-negotiables

- **Never modify anything under `_ref/`** — it is read-only reference material (the resource
  viewer/editor is unlocked only after the main port is finished). Copy what you need out of it;
  `.githooks/pre-commit` rejects any commit that changes `_ref/`.
- After *any* edit to `asm/src/swreg.asm`, run:

      sh asm/tools/verify.sh

  and require `SWREG.EXE: OK` + `IDENTICAL`. If it fails, fix or revert before anything else.
  One assembly is ≈ 60 s — batch edits to keep this cheap.
- **Never use `/tmp`.** Scratch and intermediates go to `build/tmp/`; build outputs to
  `build/<kind>/`. This applies to throwaway scripts too.
- **No build artifacts in commits.** If you must invoke an assembler manually, target `build/tmp/`.
- **No history rewriting** (amend / rebase / force-push) unless the user explicitly asks for it.
- MASM is historic only: the frozen MASM source stays in the reference (`_ref/disasm/masm/`); the project here builds with UASM (`asm/tools/build.sh` / `asm/tools/build.bat`).
- No AI/co-author attribution trailers in commits unless asked.

## Reporting

- After asm work, state the verification result explicitly (`md5 OK`, `IDENTICAL`).
- Keep the plan honest: add dated progress-log entries to `docs/porting.md` when milestones move.
- If a task needs a GUI or the user's machine (DOSBox-X runs, real hardware), say so and leave a
  runnable script plus expected output instead of guessing.
- Keep commits small and feature-scoped, Conventional Commits style:
  `<type>(<scope>): <summary>` (`feat`, `fix`, `docs`, `test`, `build`, `chore`, `refactor`).
  A feature's code and docs travel in the same commit; squash follow-up fixes into the commit
  they fix.

## Environment on this machine

- `uasm` 2.57 at `~/.local/bin/uasm` (on `PATH`).
- `python3` + `capstone`; `dosbox-x`; `gh` (authenticated as the repo owner).
- Tools read the reference via `SWREG_EXE`; e.g.
  `SWREG_EXE=_ref/orig-bin/SWREG.EXE python3 asm/tools/verify_asm.py`.

## Where things are

- Plan: `docs/porting.md` (§3 design, §4 verification, §6 risks, §10 progress log).
- Protocol: `docs/protocol.md` (service families, encoding policy, display state, recordings).
- Disassembly notes: `docs/disassembly.md` (build/verify, source conventions).
- Disassembly: `asm/` — source in `src/`, build/verify machinery in `tools/`.
- Verifier: `asm/tools/verify_asm.py` (`--help`).
- Build/verify scripts: `asm/tools/{build.sh,verify.sh,build.bat}` — outputs land in
  `build/uasm/`; the build stages the minimum game files there so the result runs as-is.
- Oracle harness: `harness/` (DOSBox-X profile + `baseline.sh`; helpers `keys.py`, `snap.py`);
  outputs in `build/oracle/`.
- Guard hook: `.githooks/pre-commit` blocks any commit touching `_ref/`.
- Reference hashes: `SWREG.EXE` MD5 `c3bdd3054e55a1e0b57c9780654dc982`.

## Current focus

Check `docs/porting.md` §10 before acting. As of the last update: M0 is done — the oracle
profile and headless baseline are in place; the annotation style guide (`docs/asm-style.md`)
and the Conventional Commits policy are in place. Next is the M1 pilot: annotate the EMS
manager / picture cache, the DOS file service, the palette and the video boundary.
