# Roadmap and open items

This is the live "what next" view of the port. The design and verification rules
are in `docs/porting.md`; the dated record is `docs/progress.md`.

## Milestones

Stage mapping: **B1 = M7** (executable spec/oracle, done), **B2 = M8** (the C
deliverable, next), host boundary = M9, **C# = M10** (the real forward effort).

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
| M6 | ✓ | Sound | FM music audible; the game's WORLD.DAT effects stream as VOC samples when sound is on (the effect-3 PC-speaker fallback stays); port-write goldens match |
| M7 | ✓ | B1 pseudo-port (executable spec) | quick-and-dirty 100% C transcription of the annotated asm runs the original end-to-end and passes the gates; typed file/save records and frozen protocol; Linux/Windows x64 builds. **A milestone, not the deliverable.** |
| M8 | … | B2 real C port | minimal-but-real, hand-written idiomatic C engine: typed game state/data and normal abstractions; no asm, no guest CPU/registers/memory, no segment:offset dispatch, no emulation glue in the product path; the B2-only build passes the differential gates. State core and save loader started in `engine/port/`. **This is the C deliverable.** |
| M9 | — | Companion client / host boundary | C core as a host-neutral library over direct/in-process, IPC (shared memory/socket) and record/replay transports; the SDL client renders, plays OPL and injects input on them (the folder link stays a debug backend); replay from recordings; input and audio scheduled on one boundary clock |
| M10 | — | C# port | new solution, written from B2 (not B1): starts as stream client/front-end, then takes over the engine; the same replay/differential gates run against it. **Starts the real forward effort.** |

## Open items

- B2 state model and module boundaries — started in `engine/port/` (live party
  state + save container); the B2 tree must not link B1 code.
- transport set for the product (in-process / shared memory / sockets) — decide
  at M9.
- codec upgrade (LZSS/LZ4) timing — only if the video link needs the bytes.

## Next

- **M8 - B2 real C port.** State core, save loader/writer, the `WORLD.DAT`
  map/palette loader, the `PICTURES.VGA` groups, the indexed surface, the static
  start-up scenes and the clue-book shell (help page, F1 map catalog and
  overhead map, F2 monster list, F3 spell list, F4 class list and spell-by-class
  details, F5 categories, item details and info screens, F6 walkthrough, F2
  monster details, F3 spell details, F1 legend selection/display, list and menu
  click parity) are in `engine/port/`, each rendering to the original's exact
  frames. B2 speaks protocol v0 over the folder link (`yt2p-play` + the
  companion client, live on Xvfb). The hintbook is complete per B1; boundary
  completion is done (codec 1, AUDIO encoders, scroll, TICK — no live audio
  source yet, M9). The boot/menu state machine and the animated story intro
  (attract loop) are ported and gated behaviorally against B1 (`check_intro.py`,
  two cycles, audio triggers exact). MORE-up/down is DOS-verified at ±14 rows,
  matching B2. The first gameplay slice (party creation + roster:
  `fn_13EC4`/`fn_1C354`/`fn_13F2E` in `engine/port/src/create.c`, wired into
  `yt2p-play`'s menu loop) is ported and gated framebuffer-exact against the 19
  M5 creation/roster checkpoints. The embark/walk slice (room renderer, explore
  session, movement) is ported and gated pixel-exact (cursor-free) plus live
  (`live_explore.py`). The in-game menu (`fn_0EBD6`: options, save/load slot
  panel) is ported with vanilla file roles (fresh boot, `CURGAME` scratch,
  `SAVGAME` slots) and gated pixel-exact plus byte-identical saves against B1.
  The combat/loot slice is in: S/A combat rounds with award side panel
  (`engine/port/src/combat.c`), chest search/prompt/modal/loot with per-action
  side-panel/compass rules, prompt cancel, live S/A/SPACE/click wiring;
  `tests/port-combat` pins 16 quiescent B1 actions and `tests/port-cancel` the
  cancel, bytes+sfx+palette+video. The explore-mouse slice is in: arrow buttons
  (forward/back/turns plus the mouse-only strafes), viewport right-click search
  (same `loc_003F3` as SPACE), the six-line currency pane with SFX 2, the
  persistent bit-8 sticky behind the gated clears, and click-time party sync;
  `live_chest.py` proves the whole mouse loop live. Live combat parity is in:
  the formal panel (quick access grid, swords icon, extended side panel,
  pixel-exact vs B1), the derived victory purse (BCD-gated currency icons), the
  A-verb button flash with injury levels and blood, the S campfire flash, formal
  clicks (verb buttons, side panel target select, quick access grid), formal D
  menu nest, and the monster pose/DoT turn; the panel bytes gate against B1
  (`formal-panel.bin`), live streams show both button states. Enemy action is
  in: the full monster melee pipeline (effect roll, all three calc arms, applier
  with HP/SP/purse/ stat/equipment legs, SFX, portrait blits, death, wipe with
  menu 0x70 + load), v_50CC weapon codes with fumble effects, plus S-missile
  flight (staging, per-band M7 flight, erase, bbox-exact vs B1). Dialogue is
  scouted (cells, modal frames, text/keyword format, file-15 NPC layout,
  `fn_07C4A` dispatch map). Next is the dialogue/NPC engine (modal, pager,
  keywords, shops/train, effects), then monster missile flight, the C cast UI,
  the P sheet UI, 1-4/portrait inspect, and the remaining explore inputs (icon
  row, message link, modal right-click, transfer/stairs arms). Animation timing
  is in (DelayTicks pacing, idle re-render, side panel exclusion). Every slice
  stays gated differentially against B1 and the DOS oracle. B1 is frozen as the
  oracle.
- M7 details: the pseudo-port is instruction-level emitted C over a 16-bit guest
  model. It is the executable spec, not the product; no new gameplay work goes
  into it.
