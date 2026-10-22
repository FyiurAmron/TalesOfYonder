Main rule: *If unsure, ask instead of pondering or guessing.*

`CONTRIBUTING.md` is the **source of truth** for layout, conventions, gates and history. Read it
first and follow it; if this file ever disagrees with it, `CONTRIBUTING.md` wins. This file only
adds agent-specific operational rules.

## Start of session

1. Read `CONTRIBUTING.md`, then `docs/porting.md` (plan + progress log) and `docs/protocol.md` (boundary spec).
2. Check `git status` and `git log --oneline -3`; work on `dev` (create it from `main` if absent),
   tree should be clean.
3. Before touching the disassembly, read `docs/disassembly.md`.
4. Make sure the guard hook is active: `git config core.hooksPath .githooks` (idempotent).

## Non-negotiables

- **Never modify anything under `_ref/`** — it is read-only reference material (the resource
  viewer/editor is unlocked only after the main port is finished). Copy what you need out of it;
  `.githooks/pre-commit` rejects any commit that changes `_ref/`.
- After *any* edit to `engine/asm/swreg/src/swreg.asm`, run:

      sh engine/asm/swreg/tools/verify.sh

  and require `SWREG.EXE: OK` + `IDENTICAL`. If it fails, fix or revert before anything else.
  One assembly is ≈ 60 s — batch edits to keep this cheap.
- **Never use `/tmp`.** Scratch and intermediates go to `build/tmp/`; build outputs to
  `build/<kind>/`. This applies to throwaway scripts too.
- **No build artifacts in commits.** If you must invoke an assembler manually, target `build/tmp/`.
- **History**: only the latest commit on `dev` may be amended/squashed without asking; see
  "Branching and pushes" for the rest.
- MASM is historic only: the frozen MASM source stays in the reference (`_ref/disasm/masm/`); the project here builds with UASM (`engine/asm/swreg/tools/build.sh` / `engine/asm/swreg/tools/build.bat`).
- No AI/co-author attribution trailers in commits unless asked.

## Branching and pushes

- `main` is the published branch; do normal work on `dev` (create it from `main` if absent).
- Push `main` only on the user's explicit command.
- Amend/squash the **latest** commit on `dev` without asking; it is the agent's call.
- Amend/squash **any older commit**, on `dev` or `main`, only on the user's explicit command.

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
  `SWREG_EXE=_ref/orig-bin/SWREG.EXE python3 engine/asm/swreg/tools/verify_asm.py`.

## Where things are

- Plan: `docs/porting.md` (§3 design, §4 verification, §6 risks, §10 progress log).
- Protocol: `docs/protocol.md` (service families, encoding policy, display state, recordings).
- Disassembly notes: `docs/disassembly.md` (build/verify, source conventions).
- Layout: `docs/layout.md` (the tree; `build/` mirrors it).
- Disassembly: `engine/asm/swreg/` — source in `src/`, build/verify machinery in `tools/`.
- Verifier: `engine/asm/swreg/tools/verify_asm.py` (`--help`).
- Build/verify scripts: `engine/asm/swreg/tools/{build.sh,verify.sh,build.bat}` — outputs land in
  `build/swreg/oracle/`; the build stages the minimum game files there so the result runs as-is.
- Oracle harness: `tools/dosbox/` (DOSBox-X profile + `baseline.sh`; helpers `keys.py`, `snap.py`);
  outputs in `build/runs/oracle/`.
- Driver: `engine/asm/sbfmdrv/` (`src/`, `tools/`, `test/`); notes in `docs/sbfmdrv.md`.
- Client: `client/` (C#) — `sh client/server.sh` for the live link,
  `sh client/publish-win.sh` for the Windows bundle. Tests: `tests/replay/run.sh` (offline),
  `tests/link/run.sh` (live); stream tooling in `tools/stream/`.
- Guard hook: `.githooks/pre-commit` blocks any commit touching `_ref/`.
- Reference hashes: `SWREG.EXE` MD5 `c3bdd3054e55a1e0b57c9780654dc982`.

## Current focus

Check `docs/porting.md` §10 before acting. As of the last update: M1 is done — the EMS manager /
picture cache, the DOS file service, the palette service and the video boundary (draw segment,
mode sets, VBE bank switch) are annotated; `docs/asm-style.md` is in place and the byte gate is
green after each batch. The optional M1b boundary spike is done as well: a `-DSTREAM` build
streams mode-13h dirty rows and palette writes to `STREAM.BIN`, and `tools/stream/replay.py`
replays them (see `docs/stream.md`). The M1 input wrappers are annotated too: keyboard
poll/wait (`fn_PollInput`/`fn_WaitKey`, DOS int 21h 06h/0Ch) and the INT 33h mouse driver with
its software cursor (`fn_MouseInit`/`fn_MouseHandler`, S13A0). The sound and tick wrappers are
annotated as well: the SBFMDRV FM service (S187E: env parsing, "FMDRV" vector scan + self-patched
`int 0`, music/SFX buffers, PC-speaker fallback) and the INT 1Ch game tick (install/restore,
ISR stack/state, direct BIOS tick reads). SBFMDRV.COM itself is now disassembled byte-perfectly
and documented in `docs/sbfmdrv.md`: installer/uninstaller, resident INT API, music state
machine, checksum self-patch, and a DOSBox-X black-box harness (`engine/asm/sbfmdrv/test/`) are in place.
The repository layout is settled (`docs/layout.md`): engine sources under `engine/`, host tooling
under `tools/`, committed gates under `tests/`, and `build/` mirrors the tree
(`build/swreg/<variant>`, `build/sbfmdrv/`, `build/client/`, `build/runs/<kind>/<stamp>`, `build/dist/`).
Next: start M2 (C core + platform layer).  The driver work is closed: real `WORLD.DAT`
blocks play against the static walk (`test/run.sh music`, with `MUSIC_CAP=wav|dro|both`
recordings), the game-level audio run is `tools/dosbox/audio.sh`, and `/U` refusing to
remove the driver from under a later TSR is gated by `test/run.sh uninstall-late`.  The
game-side `v_SoundEntry` question is resolved: the
driver has no far dispatcher, the pointer is zero, and the far calls are vestigial (treat them
as no-ops; the resident INT API is the real interface).
