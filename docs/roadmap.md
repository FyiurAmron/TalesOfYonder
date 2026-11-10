# Roadmap and open items

This is the live "what next" view of the port.  The design and verification
rules are in `docs/porting.md`; the dated record is `docs/progress.md`.

## Milestones

| # | Done | Deliverable | Done when |
|---|---|---|---|
| M0 | ✓ | Setup & baseline | git repo + ignores; UASM build verified on Linux (MD5); DOSBox-X profile runs the original; `verify_asm.py` works; style guide + stream format v0 drafted |
| M1 | ✓ | Annotated-asm pilot | 2–3 subsystems annotated (EMS manager, file manager, palette) with byte-verify green |
| M1b | ✓ | Boundary spike (optional) | one seam (video present/palette) streamed from the asm build to a minimal client; proves protocol + showcase path; live client link, see `docs/client.md` |
| M1c | ✓ | Timed asm session trace | capture/replay both directions with paced output and input |
| M2 | ✓ | C core + platform service layer | memory model and service boundary; headless/recording/folder backends run a stub program that writes timed `OUTPUT.BIN` and reads `INPUT.BIN`; the existing C# client renders them, see `engine/c/README.md` |
| M3 | ✓ | First light | emitted C reaches title/attract; checkpoint hashes match oracle on the no-input path; the same engine is playable through the live folder link (`yt2game --live`, mode 13h) |
| M4 | ✓ | Subsystem migration | all non-sound modules ported + tested in dependency order (`docs/porting.md` §3); streams diff against TRACE builds |
| M5 | ✓ | Gameplay slice | scripted char-creation → walking → combat session matches oracle; extended through the spider victory and chest loot |
| M6 | ✓ | Sound | FM music audible; SFX path resolved (SBFMDRV carries music only in this chapter; the effect-3 PC-speaker fallback is streamed and rendered); port-write goldens match |
| M7 | — | C code cleanup & packaging | typed structures, 16-bit artifacts removed; Linux/Windows builds; spec frozen |
| M8 | — | Companion client | C core as a host-neutral library over direct/in-process, IPC (shared memory/socket) and record/replay transports; the SDL client renders, plays OPL and injects input on them (the folder link stays a debug backend); replay from recordings; input and audio scheduled on one boundary clock |
| M9 | — | C# bootstrap | new solution; starts as stream client/front-end; mechanical translation of frozen C passes replay tests |

## Open items

- transport set for the product (in-process / shared memory / sockets) — decide at M8.
- codec upgrade (LZSS/LZ4) timing — only if the video link needs the bytes.

## Next

- **Optional save/load button test.**  The save data already matches
  (`CURGAME`); a small UI regression test can save to a `SAVEGAMEX` slot,
  load it back and check the party state.
- **M7 - C code cleanup and packaging.**  Typed structures, 16-bit artifacts removed,
  Linux/Windows builds and the frozen spec.
