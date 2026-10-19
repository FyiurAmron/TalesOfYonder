# YT2 — Yendorian Tales Book I, Chapter 2: port plan (v1, draft)

Recreation of `SWREG.EXE` (201,810 bytes, MD5 `c3bdd3054e55a1e0b57c9780654dc982`) for modern
platforms. This document is the agreed working plan; it supersedes ad-hoc notes.

## 0. Assets in the repo

| Path | Role |
|---|---|
| `_ref/orig-bin/` | original distribution: `SWREG.EXE` (MD5 `c3bdd3…`), `WORLD.DAT`, `PICTURES.VGA`, `SBFMDRV.COM`, `CH2.BAT`, `CURGAME`, `README.DOC`, `YENDOR2.BMP` |
| `asm/` | the working byte-perfect disassembly: source in `asm/src/`, build/verify machinery in `asm/tools/` (sourced from the reference below) |
| `_ref/disasm/` | frozen read-only reference: generation pipeline, historic MASM source, toolchain evidence |
| `_ref/TalesOfYonder/` | data-format mining project (existing C#, moved in from this repository's previous life): picture/palette/world decoding; **cross-reference and future GUI resource editor** (read-only until the main port is finished), not the engine |

Conventions: `_ref/` is read-only reference material (enforced by the `.githooks/pre-commit` guard);
the living disassembly lives in `asm/` (`src/` + `tools/`). Build outputs go
under `build/` (`build/uasm/`, ...; the whole directory is ignored); scratch files go to `build/tmp/`
(not `/tmp`). Rules for contributors/agents: `CONTRIBUTING.md` and `AGENTS.md`.

`asm/` (working) and `_ref/disasm` (frozen reference) facts:

- `asm/src/swreg.asm` (working copy) rebuilds byte-for-byte with UASM 2.53+ (`tools/build.sh`: `uasm -mz` +
  `fixhdr.py`, then MD5 check). **This is the living source.**
- the frozen baselines `_ref/disasm/uasm/swreg.asm` and `_ref/disasm/masm/swreg.asm` (ML 6.11d +
  LINK 5.31 `/SE:512`) rebuild byte-for-byte as well.
- `_ref/disasm/tools/*.py` regenerates the asm from the exe: Capstone recursive tracer (`trace.py`),
  coverage / table / segment model (`model.py`, `model2.py`, `tables.py`), emitter (`emit.py`).
  Symbols: 381 `fn_`, 419 `sub_`, 3,811 `loc_`, 4 dispatch tables, 512 `v_` variables.

Program shape: ~122 KB code (~35–36k instructions, 133 byte-aligned segments, 3,913 relocations),
63 KB static data image; all resident, no overlays.

### External surface (everything a platform layer must replace)

| Area | Mechanism | Notes |
|---|---|---|
| Files | int 21h | "file struct" blocks at DS:9040 (`CURGAME`, `PICTURES.VGA`, `SAVGAMEX`, `WORLD.DAT`) |
| Memory | int 21h 48h/49h/4Ah, **int 67h EMS** | EMS 4.0 required; allocates 512 KB–6 MB for the picture cache; page frame in a variable; some rendering paths use the EMS window |
| Video | int 10h, ports 3C8/3C9/3DA | mode 13h (320×200×256) and VBE 1.2 mode 101h (640×480×256) with 4F02/4F05 banking; palette via 1012h/1017h and DAC ports; retrace sync via 3DA. All drawing goes through a mutable screen-segment variable |
| Keyboard | int 21h 06h/0Ch | scancode dispatch tables in `S0000` (0x81A, 0x8DA) |
| Mouse | int 33h | standard driver + game-installed user event handler (AX=000Ch) — callbacks into game code |
| Time | int 1Ah, int 1Ch hook | tick reads / game tick handler (saves+restores vectors) |
| Sound | **int 00h only** | Creative `SBFMDRV.COM` FM driver 1.34 installs itself at INT 00h; game parses `BLASTER=`/`SOUND=`, pre-inits driver, then calls int 0. No SB/PIT/DMA I/O in game code |
| Data | DS + files | DS holds the whole RPG text/UI/NPC data; `WORLD.DAT` level DB; `PICTURES.VGA` picture groups |

## 1. Strategy: staged port, one testable spine

```
asm (byte-perfect)  ->  annotated asm (byte-perfect, readable)  ->  portable C (faithful)  ->  C#
     oracle                  human-facing spec                       executable spec         product
   (DOSBox-X)              (names/comments/macros)                (generated, then refined)
                                                                  frontend may live out-of-process (§5)
```

Rules that make it safe:

1. **The rebuild + MD5 check is the invariant at every step.** Any asm edit is accepted only if
   `asm/tools/build.sh` still produces the exact bytes.
2. **Verification is differential**: original (DOSBox-X + instrumented builds) vs C port, at
   unit / module / end-to-end levels.
3. **Annotations flow forward**: names, comments, struct layouts and EQU constants added to the
   asm become names in the C, and later names in the C#.
4. **No semantics are changed while restructuring.** Behaviour changes are a separate, later,
   test-backed activity.
5. **Service boundaries are message-shaped and recordable.** Every video/palette, audio/OPL,
   input and tick service must be replayable as a stream: first backend in-process, a recording
   backend early (the harness uses it), companion transports/clients later (§5).

## 2. Stage A — byte-perfect *annotated* assembly (new)

`asm/src/swreg.asm` becomes the hand-maintained primary source (frozen baseline from the generator
kept for fallback; `masm/` stays untouched).

Annotation work, done **just-in-time per module** (annotate what you are about to port):

- semantic names: `fn_07010` → `fn_LoadWorldData`, `v_50CC` → `v_GameState`,
  `tbl_...` for dispatch tables; keep `loc_XXXXX` as flow labels;
- `EQU` constants for struct field offsets (file structs, EMS handles, widget records, actor records);
- comments: purpose, register conventions, in/out, side effects, "called from" hints;
- macros only where an idiom repeats **and** expands to the exact proven encodings
  (e.g. existing `x_cmp_*` macros); never force macros over varied register setups;
- module banners grouping segments into subsystems.

Verification loop (new tool `tools/verify_asm.py`):

1. `uasm -mz -Fl` build → MD5 must match;
2. parse the listing and compare **every instruction's bytes** against the original image
   (the `check()` logic in `tools/build.py` generalised, address-keyed per instruction);
3. commit. Any mismatch → revert that batch.

The UASM listing (`-Fl`, macro-expanded, address+bytes per line) is the intended parseable input
for the C emitter — macros are pre-expanded and every line is byte-attributable.

Harness builds: `%ifdef HARNESS` blocks in the same source (default build unaffected) for
scripted input injection and checkpoint dumps; `%ifdef TRACE` for sound port-write logging.

## 3. Stage B — portable C ("faithful reincarnation")

Target: C11, gcc/clang/msvc, no dependencies besides SDL3. Runs on >= 32-bit systems.

Memory & CPU model:

- `uint8_t mem[0x110000]`; `seg:off → seg*16 + off`; VGA RAM (0xA0000) and the EMS window live in
  that array, so all original addressing survives unchanged;
- registers/flags explicit (`uint16_t`, 286 semantics, wrap casts); helpers for ALU ops;
  `jmp ax` tables → `switch`; CS-resident data slots → globals;
- one C function per `fn_`/`sub_`; `goto` for `loc_`; far pointers → `uint32_t` + access helpers.

Platform layer (the only modern code):

- files: DOS-ish semantics (8.3, case-insensitive, wildcards) behind the service interface;
  engine-local shim while porting, flipped to the client as the final layer step (§5);
- EMS: arena + 64 KB window emulation (int 67h 40h–51h implemented);
- video: windowed linear framebuffer + palette + SDL2 present (texture with 8-bit palette);
- input: scripted events (harness) and SDL2 keyboard/mouse mapped onto the existing dispatch
  tables / int 33h equivalents, incl. invoking the game's user handler;
- clock: logical 18.2 Hz tick; `int 1Ah`/int 1Ch served from it → deterministic replay;
- sound: OPL2/3 core + reimplemented `SBFMDRV` INT 00h protocol (see §7);
- record/replay + transport backends: the same service calls can be logged to a stream, replayed,
  or forwarded to a companion client (§5).

Emission pipeline (`tools/port/`): UASM listing + Model (coverage/kinds/tables) → IR →
C files per subsystem. Generated first, then **refined routine-by-routine as tests pass**.
C style constraint: a subset that maps 1:1 to C# (explicit widths, no UB, index-based memory
access, no pointer aliasing tricks) so the C# pass can start as mechanical translation.

Module order: init/entry & DOS services → EMS manager → video/palette → world & picture loaders →
input/menus → tick/timing → gameplay slice → saves → sound. Disk I/O is a thin engine-local shim
during the port and moves to the client as the final layer step.

C# endgame: **new project** (not TalesOfYonder). TalesOfYonder stays the data-format reference and
becomes the GUI resource editor. The C# app starts as a companion client over the same stream
(§5), then takes over the engine; the same replay tests run against it.

## 4. Verification spine

- Oracle: DOSBox-X + original exe + `SBFMDRV.COM` + EMS/mouse/SB config; save-states for retries.
- Instrumented uasm builds: scripted input (keystrokes/mouse) and checkpoint hashes
  (DS image, video window, key globals) written to files; identical scripts drive the C port.
- Three levels: unit (C tests after refactor) / module (DOS stub builds for EMS, loaders, palette,
  sound commands) / end-to-end scripted playthroughs with checkpoint diffs.
- Byte-comparable artifacts: save files, file outputs, palette/picture decode paths.
- Sound: record the driver's OPL register-write stream in DOSBox-X; compare the C port's stream
  for identical music commands (also how the INT 00h protocol gets black-boxed).
- **Stream diffing**: instrumented original builds emit the same service-event stream (video
  deltas + palette, input, OPL writes, ticks) that the C port emits; diffing streams localises
  translation bugs that checkpoint hashes only localise to "somewhere before here".
- Recordings are two-directional session captures (input + video/audio events + FS responses):
  self-contained replays need no data files; pinned-hash mode keeps CI fast.
- Determinism: logical clock + scripted input + located/scripted RNG seeding.

## 5. Companion client / thin-frontend mode

The engine can run with no in-process frontend: video/palette, audio and input cross a boundary to
a separate **companion client** that renders, plays sound and injects input. This is the same
service seam as §3 — a client is just another service backend. Message details: `docs/protocol.md`.

- **Engine side owns**: game logic, memory, EMS, files, logical clock, determinism.
- **Client side owns**: display state (framebuffer replica, palette, mode), window/scaling/filters,
  OPL synthesis (register writes cross the wire, not PCM), input capture, debug overlays.
- **Message families** (dead simple, 16-bit-friendly): video deltas (dirty rects) + palette writes;
  OPL register writes; input events (client -> engine); tick/pacing; tiny control ops.
- **Transports**: in-process/direct (default), shared memory, TCP/Unix sockets. No hardware or
  baud constraints — the protocol is transport-independent; anything else is a trivial tunnel.
- **Why it earns its keep beyond the showcase**: (a) the record/replay backend is the test harness
  from day one; (b) streams from instrumented original builds diff against C-port streams —
  localised translation debugging; (c) clients are developed against recordings, in parallel with
  engine work; (d) the C# front-end starts life as a client (frames, inspectors, resource editor).
- **Client to the end**: the split is architecture, not scaffolding. In the C# product the engine
  is a core library with a formal host interface (same messages) and the app is host/client;
  in-process is just the degenerate transport. Tests target the boundary (record/replay).
- **Disk I/O joins the client last**: while porting, files stay engine-local (simple testing);
  the final layer step flips the FS service to the client, which implements DOS-ish semantics
  (8.3, case-insensitivity, wildcards, EOF) over host files. Engine keeps name/offset semantics
  only. Both directions are recorded, so replays stay self-contained.
- **asm-level variant**: the annotated asm already draws through a screen-segment variable and
  funnels I/O through a handful of wrappers (present/retrace wait, two palette loaders, mode set,
  key wrapper, mouse wrapper, sound wrapper), so "replace only the lowest boundary" is a bounded
  asm edit, not a rewrite. The byte-perfect build stays as the oracle; the stream build is
  validated against it (stream + checkpoint diffing).
- **Reality checks**: video is delta-encoded (dirty rects + row-skip RLE) and audio travels as OPL
  register writes, not PCM — cheap by construction; input latency is bounded by poll-per-frame
  queueing (fine for a turn-based RPG); the client never owns *game* state (it owns host
  resources: window, audio, later the filesystem).
- **Scheduling**: stream format + recording backend early (M2); one asm seam spike (M1b, optional);
  full companion client at M8; C# client at M9.

## 6. Milestones

| # | Deliverable | Done when |
|---|---|---|
| M0 | Setup & baseline | git repo + ignores; UASM build verified on Linux (MD5); DOSBox-X profile runs the original; `verify_asm.py` works; style guide + stream format v0 drafted |
| M1 | Annotated-asm pilot | 2–3 subsystems annotated (EMS manager, file manager, palette) with byte-verify green |
| M1b | Boundary spike (optional) | one seam (video present/palette) streamed from the asm build to a minimal client; proves protocol + showcase path |
| M2 | C core + platform layer | memory model, service layer, SDL3 video/input/clock run a stub program; recording backend writes replayable streams |
| M3 | First light | emitted C reaches title/attract; checkpoint hashes match oracle on the no-input path |
| M4 | Subsystem migration | each module ported + tested in dependency order (§3); streams diff against TRACE builds |
| M5 | Gameplay slice | scripted char-creation → walking → combat session matches oracle |
| M6 | Sound | FM music + SFX audible; port-write goldens match; digital path documented |
| M7 | C cleanup & packaging | typed structures, 16-bit artifacts removed; Linux/Windows builds; spec frozen |
| M8 | Companion client | SDL client renders/plays/injects over in-process + TCP; replay from recordings; disk I/O flipped to the client (final layer step) |
| M9 | C# bootstrap | new solution; starts as stream client/front-end; mechanical translation of frozen C passes replay tests |

## 6. Risks / unknowns (ordered)

1. **SBFMDRV protocol / music data source** — the only fuzzy interface. 7 KB driver: disassemble +
   black-box via port-write goldens; worst case, a captured-write-stream implementation suffices
   for v1.
2. **EMS window and 64K wrap semantics** — mitigated by flat memory + explicit `uint16_t` casts;
   module tests with fuzzed page mappings.
3. **Flag modelling** in generated C — conservative flag updates first, dataflow-prune later.
4. **Callback inversion** (mouse user handler, int 1Ch) — platform must call into game code;
   keep an explicit registry.
5. **Timing-dependent logic** — logical clock + oracle lockstep; budget fix-up time.
6. **Text/console emulation** (DOS string conventions, screens) — small dedicated service in C.
7. **Scope creep in stage C** — don't refactor ahead of tests; the faithful layer is the spec.
8. **Companion-mode scope** — keep the first client thin (window, palette, OPL audio, input);
   transports only after the protocol is proven on one seam.
9. **Protocol efficiency** — dirty-rect streaming + codec field from day one; OPL register writes,
   not PCM; keep codecs trivial and shared (raw, RLE-skip; LZ later); no hardware/baud targets.

Non-goals: emulator-in-a-box product, Windows-only runtime, engine/editor frameworks before the
faithful C layer is stable, remote display of the unmodified binary (dropped — that is an
emulator-hook demo, not this port).

## 7. Immediate next actions (M0 checklist)

1. `git init` + `.gitignore` (`*.obj`, `*.lst`, built `SWREG.EXE`, `fb.pkl`, `__pycache__`).
2. Install/get UASM for Linux; run `asm/tools/verify.sh` (outputs to `build/uasm/`, staged with
   the minimum game files); confirm MD5 `c3bdd3…`.
3. `asm/tools/verify_asm.py` (per-instruction byte diff vs `_ref/orig-bin/SWREG.EXE`).
4. DOSBox-X profile: EMS, mouse, SB16/OPL3, `SBFMDRV` load, `CH2.BAT`; capture a baseline
   save-state and a no-input run (configs in `harness/`, outputs in `build/`).
5. Draft the annotation style guide (`docs/asm-style.md`), including the service/stream
   interface rules (§5).
6. Draft stream format v0 (message families, record framing) — it doubles as the harness format.
7. Start M1 pilot on the EMS manager (0x0233x) — highest-risk/most-instructive first.

## 8. Open items

- final decision on annotation granularity per module (functions + variables + structs; flow
  labels untouched).
- whether `masm/` gets the same annotations later (probably not — frozen historical artifact).
- transport set for the product (in-process / shared memory / sockets) — decide at M8.
- codec upgrade (LZSS/LZ4) timing — decide when FS moves client-side.
- SDL3-DOS build target as a future option (skipped for now; the companion design reduces its value).
- choice of C# binding at M9 (SDL2-CS / SDL3 bindings / hand-rolled P/Invoke) — deferred.

## 10. Progress log

- 2026-10-19 — M0 (partial):
  - the repository continues the old TalesOfYonder history (`1d30489`); a single "rebirth" bootstrap
    commit sits on top of it; branch `main` pushed, remote default `main`, `master` removed;
    contributor/agent rules added (`CONTRIBUTING.md`, `AGENTS.md`);
  - `_ref/` is the read-only reference (original distribution, frozen disassembly pipeline,
    vendored mining project); a pre-commit hook (`.githooks/`) refuses commits that change it;
  - the working disassembly lives directly in `asm/` (source in `src/`, machinery in `tools/`); MASM
    stays reference-only; `tools/build.sh` assembles into `build/uasm/` and stages the
    minimum game files (from `_ref/orig-bin/`) so the built game runs as-is;
  - build outputs routed to `build/` (scratch in `build/tmp/`); `.gitignore` covers `build/`;
  - UASM 2.57 (linux64 release) at `~/.local/bin/uasm`; `asm/tools/verify.sh` reproduces
    `SWREG.EXE` byte-identically into `build/uasm/` (0 warnings, ~65 s per assembly);
  - `asm/tools/verify_asm.py`: per-instruction byte diff vs the reference; tested positive
    (identical) and negative (localised instruction + data range);
  - base tree settled: the disassembly sits directly in `asm/` (no per-target wrapper); files grouped
    by kind — `asm/` is `src/` (source) + `tools/` (machinery), all docs live in `docs/`; the repo
    keeps `_ref/`, `docs/`, `build/` at the top level.
- 2026-10-19 — M0 oracle (done):
  - `harness/`: the oracle profile (`dosbox-x.conf` — svga_s3/VBE 1.2, EMS+XMS+UMB, SB16+OPL3,
    16 MB, fixed 30 000 cycles, autosave of save states), the headless runner (`baseline.sh` —
    Xvfb, `CH2.BAT`, original distribution from `_ref/orig-bin/`, md5-checked per run) and X11
    helpers (`keys.py` — XTEST key injection via ctypes; `snap.py` — `XGetImage` window snapshots);
  - baseline (no input) captured under `build/oracle/<stamp>/`: 3 screen snapshots (title screen,
    intro), 4 save states (slots 1–4, 15 s interval), DOSBox-X logs, `SWREG.EXE: OK`;
  - quirk: DOSBox-X 2026.01.02's own screenshot (F12+P) aborts on the game's VESA mode (libpng
    misuse) — snapshots come from the X window instead; Xvfb falls back to abstract sockets on
    this host (the `/tmp/.X11-unix` listener errors in `xvfb.log` are benign).

