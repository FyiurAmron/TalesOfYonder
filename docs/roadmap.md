# Roadmap and open items

This is the live "what next" view of the port.  The design and verification
rules are in `docs/porting.md`; the dated record is `docs/progress.md`.

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

- B2 state model and module boundaries — started in `engine/port/` (live party state + save container); the B2 tree must not link B1 code.
- transport set for the product (in-process / shared memory / sockets) — decide at M9.
- codec upgrade (LZSS/LZ4) timing — only if the video link needs the bytes.

## Next

- **M8 - B2 real C port.**  State core, save loader/writer, the `WORLD.DAT`
  map/palette loader, the `PICTURES.VGA` groups, the indexed surface, the
  static start-up scenes and the clue-book shell (help page, F1 map catalog
  and overhead map, F2 monster list, F3 spell list, F4 class list and
  spell-by-class details, F5 categories, item details and info screens,
  F6 walkthrough, F2 monster details, F3 spell details, F1 legend
  selection/display, list and menu click parity) are in `engine/port/`,
  each rendering to the original's exact frames.  B2 speaks protocol v0
  over the folder link (`yt2p-play` + the companion client, live on
  Xvfb).  The hintbook is complete per B1; next is boundary completion
  (codec 1, AUDIO), plus DOS re-verifies when X11 recovers (MORE-up);
  every slice is gated differentially against B1 and the DOS oracle. B1 is
  frozen as the oracle.
- M7 details: the pseudo-port is instruction-level emitted C over a 16-bit
  guest model. It is the executable spec, not the product; no new gameplay
  work goes into it.
