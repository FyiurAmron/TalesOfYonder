# YT2 — Yendorian Tales Book I, Chapter 2: port plan (v1, draft)

Recreation of `SWREG.EXE` (201,810 bytes, MD5 `c3bdd3054e55a1e0b57c9780654dc982`) for modern
platforms. This document is the agreed working plan; it supersedes ad-hoc notes.

## 0. Assets in the repo

| Path | Role |
|---|---|
| `_ref/orig-bin/` | original distribution: `SWREG.EXE` (MD5 `c3bdd3…`), `WORLD.DAT`, `PICTURES.VGA`, `SBFMDRV.COM`, `CH2.BAT`, `CURGAME`, `README.DOC`, `YENDOR2.BMP` |
| `engine/asm/swreg/` | the working byte-perfect disassembly: source in `engine/asm/swreg/src/`, build/verify machinery in `engine/asm/swreg/tools/` (sourced from the reference below) |
| `_ref/disasm/` | frozen read-only reference: generation pipeline, historic MASM source, toolchain evidence |
| `_ref/TalesOfYonder/` | data-format mining project (existing C#, moved in from this repository's previous life): picture/palette/world decoding; **cross-reference and future GUI resource editor** (read-only until the main port is finished), not the engine |

The full tree and the rules that keep it in that shape are in `docs/layout.md`.

Conventions: `_ref/` is read-only reference material (enforced by the `.githooks/pre-commit` guard);
the living disassembly lives in `engine/asm/swreg/` (`src/` + `tools/`). Build outputs go
under `build/` (`build/swreg/oracle/`, ...; the whole directory is ignored); scratch files go to `build/tmp/`
(not `/tmp`). Rules for contributors/agents: `CONTRIBUTING.md` and `AGENTS.md`.

`engine/asm/swreg/` (working) and `_ref/disasm` (frozen reference) facts:

- `engine/asm/swreg/src/swreg.asm` (working copy) rebuilds byte-for-byte with UASM 2.53+ (`tools/build.sh`: `uasm -mz` +
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
| Keyboard | int 21h 06h/0Ch | per-frame `fn_PollInput` + blocking `fn_WaitKey`; ASCII dispatch `tbl_KeyDispatch` (081Ah) and extended-key `tbl_ScanDispatch` (08DAh) in `S0000` |
| Mouse | int 33h | `fn_MouseInit` probes/resets the driver and installs `fn_MouseHandler` (AX=000Ch; callback inversion); 16x16 software cursor saved/restored in A000h; click coordinates in `v_Mouse*` |
| Time | int 1Ah, int 1Ch hook | tick reads / game tick handler (saves+restores vectors) |
| Sound | driver INT (self-patched) + far entry | Creative `SBFMDRV.COM` FM driver 1.34 is loaded by `CH2.BAT`; the game parses `BLASTER=`/`SOUND=`, scans vectors 80h..BEh for the driver's "FMDRV" signature, patches its own `int 0` stub (`sub_SoundCall`, 18B80h) to that vector, pre-inits through a far entry and reads music/SFX blocks from `WORLD.DAT`. The resident API, installer and music engine are mapped in `docs/sbfmdrv.md`. Fallback: PC-speaker beep (PIT ch2 + port 61h) for effect 3 when SFX are off |
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
   `engine/asm/swreg/tools/build.sh` still produces the exact bytes.
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

`engine/asm/swreg/src/swreg.asm` becomes the hand-maintained primary source (frozen baseline from the generator
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

The final assembly-server milestone is a **timed, bidirectional session trace**
against the companion client. A single monotonic clock at the client boundary
timestamps each complete engine-to-client and client-to-engine frame. The trace
keeps the asm server's output and the input that drove it together; replay paces
output, and input replay can drive another asm server run. Finish this reference
harness before starting Stage B.

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
- video: windowed linear framebuffer + palette + SDL3 present (texture with 8-bit palette);
- input: scripted events (harness) and SDL3 keyboard/mouse mapped onto the existing dispatch
  tables / int 33h equivalents, incl. invoking the game's user handler;
- clock: logical 18.2 Hz tick; `int 1Ah`/int 1Ch served from it → deterministic replay;
- sound: OPL2/3 core + reimplemented `SBFMDRV` INT 00h protocol (see `docs/sbfmdrv.md`);
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
- Recordings are timed two-directional session captures: both client inputs and
  engine outputs use one monotonic session clock. The game directory stays next
  to the engine, and data-file hashes keep a replay pinned.
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
  The asm seam ships on a shared folder (`STREAM.BIN`/`INPUT.BIN`, `docs/client.md`); the earlier
  COM1/nullmodem link is obsolete and kept only for fun.
- **Why it earns its keep beyond the showcase**: (a) the record/replay backend is the test harness
  from day one; (b) streams from instrumented original builds diff against C-port streams —
  localised translation debugging; (c) clients are developed against recordings, in parallel with
  engine work; (d) the C# front-end starts life as a client (frames, inspectors, resource editor).
- **Client to the end**: the split is architecture, not scaffolding. In the C# product the engine
  is a core library with a formal host interface (same messages) and the app is host/client;
  in-process is just the degenerate transport. Tests target the boundary (record/replay).
- **Disk I/O stays engine-local**: the folder transport already gives the engine the host
  directory, so a file service over the link would add a second filesystem layer without moving
  the game data anywhere. Replays keep the game directory next to the engine and check its hashes.
- **asm-level variant**: the annotated asm already draws through a screen-segment variable and
  funnels I/O through a handful of wrappers (present/retrace wait, two palette loaders, mode set,
  key wrapper, mouse wrapper, sound wrapper), so "replace only the lowest boundary" is a bounded
  asm edit, not a rewrite. The byte-perfect build stays as the oracle; the stream build is
  validated against it (stream + checkpoint diffing).
- **Reality checks**: video is delta-encoded (dirty rects + row-skip RLE) and audio travels as OPL
  register writes, not PCM — cheap by construction; input latency is bounded by poll-per-frame
  queueing (fine for a turn-based RPG); the client never owns *game* state (it owns host
  resources: window and audio).
- **Scheduling**: validate the boundary and recording backend against the asm
  server first (M1b/M1c); start Stage B only after the timed bidirectional
  reference trace is usable. Full companion client at M8; C# client at M9.

## 6. Milestones

| # | Deliverable | Done when |
|---|---|---|
| M0 | Setup & baseline | git repo + ignores; UASM build verified on Linux (MD5); DOSBox-X profile runs the original; `verify_asm.py` works; style guide + stream format v0 drafted |
| M1 | Annotated-asm pilot | 2–3 subsystems annotated (EMS manager, file manager, palette) with byte-verify green |
| M1b | Boundary spike (optional) | one seam (video present/palette) streamed from the asm build to a minimal client; proves protocol + showcase path — **done 2026-10-19**, see `docs/stream.md`; **live client link since 2026-10-20**, see `docs/client.md` |
| M1c | Timed asm session trace | **done 2026-10-25** - capture/replay both directions with paced output and input |
| M2 | C core + platform service layer | memory model and service boundary; headless/recording/folder backends run a stub program that writes timed `OUTPUT.BIN` and reads `INPUT.BIN`; the existing C# client renders them |
| M3 | First light | emitted C reaches title/attract; checkpoint hashes match oracle on the no-input path |
| M4 | Subsystem migration | each module ported + tested in dependency order (§3); streams diff against TRACE builds |
| M5 | Gameplay slice | scripted char-creation → walking → combat session matches oracle |
| M6 | Sound | FM music audible; SFX path resolved (SBFMDRV carries music only in this chapter; the effect-3 PC-speaker fallback is streamed and rendered); port-write goldens match |
| M7 | C cleanup & packaging | typed structures, 16-bit artifacts removed; Linux/Windows builds; spec frozen |
| M8 | Companion client | SDL client renders/plays/injects over in-process + TCP; replay from recordings; input and audio in sync |
| M9 | C# bootstrap | new solution; starts as stream client/front-end; mechanical translation of frozen C passes replay tests |

## 7. Risks / unknowns (ordered)

1. **SBFMDRV protocol / music data source** — the INT half is now mapped. The game-side half is
   annotated (driver vector found by signature scan and patched into `sub_SoundCall`; the
   music/SFX blocks travel from `WORLD.DAT`, header fields at +6/+8/+0Ch/+14h/+24h feed the
   commands), the driver is disassembled byte-perfectly, and its installer, resident INT API
   (BX 0..F and -1..-5 through the dispatcher at CS:12E6h), checksum/self-patch and music state
   machine are documented in `docs/sbfmdrv.md`; a DOSBox-X black-box run (`engine/asm/sbfmdrv/test/`)
   confirms the command semantics and a real music start. The far-entry question is resolved:
   `v_SoundEntry` is zero, has no MZ relocation and no writer; the game does reach the far-call
   site, but the call goes to `0000:0000` (which executes the IVT bytes, not an INT 0 dispatch),
   and SBFMDRV 1.34 has no far dispatcher at all. The far path is a vestigial/other-driver API;
   the resident INT API is the driver's only interface (see `docs/sbfmdrv.md` section 7). The
   stream/OPL event decode and an audio-captured game run are done as well (`docs/sbfmdrv.md`
   6.1/6.2; `tools/dosbox/audio.sh` reports 267 note-ons on the tick grid). Worst case still
   stands only as a fallback: a captured-write-stream implementation suffices for v1.
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

## 8. Open items

- final decision on annotation granularity per module (functions + variables + structs; flow
  labels untouched).
- whether `masm/` gets the same annotations later (probably not — frozen historical artifact).
- transport set for the product (in-process / shared memory / sockets) — decide at M8.
- codec upgrade (LZSS/LZ4) timing — only if the video link needs the bytes.
- SDL3-DOS build target as a future option (skipped for now; the companion design reduces its value).
- choice of C# binding at M9 — settled in practice: the bootstrap client hand-rolls SDL3 P/Invoke
  (`client/Sdl.cs`) and stays dependency-free; revisit only if the product needs more of SDL.

## 9. Progress log

- 2026-10-19 — M0 (partial):
  - the repository continues the old TalesOfYonder history (`1d30489`); a single "rebirth" bootstrap
    commit sits on top of it; branch `main` pushed, remote default `main`, `master` removed;
    contributor/agent rules added (`CONTRIBUTING.md`, `AGENTS.md`);
  - `_ref/` is the read-only reference (original distribution, frozen disassembly pipeline,
    vendored mining project); a pre-commit hook (`.githooks/`) refuses commits that change it;
  - the working disassembly lives directly in `engine/asm/swreg/` (source in `src/`, machinery in `tools/`); MASM
    stays reference-only; `tools/build.sh` assembles into `build/swreg/oracle/` and stages the
    minimum game files (from `_ref/orig-bin/`) so the built game runs as-is;
  - build outputs routed to `build/` (scratch in `build/tmp/`); `.gitignore` covers `build/`;
  - UASM 2.57 (linux64 release) at `~/.local/bin/uasm`; `engine/asm/swreg/tools/verify.sh` reproduces
    `SWREG.EXE` byte-identically into `build/swreg/oracle/` (0 warnings, ~65 s per assembly);
  - `engine/asm/swreg/tools/verify_asm.py`: per-instruction byte diff vs the reference; tested positive
    (identical) and negative (localised instruction + data range);
  - base tree settled: the disassembly sits directly in `engine/asm/swreg/` (no per-target wrapper); files grouped
    by kind — `engine/asm/swreg/` is `src/` (source) + `tools/` (machinery), all docs live in `docs/`; the repo
    keeps `_ref/`, `docs/`, `build/` at the top level.
- 2026-10-19 — M0 oracle (done):
  - `tools/dosbox/`: the oracle profile (`dosbox-x.conf` — svga_s3/VBE 1.2, EMS+XMS+UMB, SB16+OPL3,
    16 MB, fixed 30 000 cycles, autosave of save states), the headless runner (`baseline.sh` —
    Xvfb, `CH2.BAT`, original distribution from `_ref/orig-bin/`, md5-checked per run) and X11
    helpers (`keys.py` — XTEST key injection via ctypes; `snap.py` — `XGetImage` window snapshots);
  - baseline (no input) captured under `build/runs/oracle/<stamp>/`: 3 screen snapshots (title screen,
    intro), 4 save states (slots 1–4, 15 s interval), DOSBox-X logs, `SWREG.EXE: OK`;
  - quirk: DOSBox-X 2026.01.02's own screenshot (F12+P) aborts on the game's VESA mode (libpng
    misuse) — snapshots come from the X window instead; Xvfb falls back to abstract sockets on
    this host (the `/tmp/.X11-unix` listener errors in `xvfb.log` are benign).
- 2026-10-19 — M1 pilot (EMS manager annotated):
  - `docs/asm-style.md`: annotation rules — prefixes + PascalCase names, original offsets kept on
    routine headers and variable definitions, module/routine headers, `; SEAM <family>: ...` tags,
    workflow and the verify gate;
  - `engine/asm/swreg/src/swreg.asm`: the EMS / picture-cache module (S0233) annotated — EMS init and close,
    page-list mapping, cache sizing by available pages, class-table construction, the DOS
    paragraph allocator; 22 symbols named (`fn_EMSInit`, `v_EMSFrameSeg`, `v_CacheSlots0`...);
  - structure found: ten picture classes (318x198 down to 8x8) with `size = width*height` and a
    32-bit PICTURES.VGA offset per class; the cache handle holds the images, one 6-byte
    {0FFFFh, page, offset} descriptor per slot;
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — M1 file service and palette (annotated):
  - DOS file service (S084E, plus the tick delay in S084D): close, open, create, read and write
    wrappers over one block-seek helper; open mode comes in AL (0 = read, 2 = read/write), a
    failed wrapper leaves the caller's error code in `v_50CC` for the common error reporter;
  - four packed file structs in DGROUP — handle, buffer seg/off, block size, block index, 32-bit
    base offset, ASCIIZ name — described by `FILE_*` EQUs at the top of the file; access is
    `base + index * block size`, one block per call; bases: 904Bh `CURGAME`, 9061h
    `PICTURES.VGA`, 907Ch `SAVEGAMEX`, 9093h `WORLD.DAT`;
  - palette service (S15B7): a pass-based fade engine (modes 0–5) over four 768-byte buffers —
    4124h live/DAC source, 4424h target (read from data files), 4754h fade-in staging, 4D56h
    work buffer; `fn_FadeOut`/`fn_FadeIn` wrap modes 0/1 (63 passes x 256 entries); DAC seam
    via ports 3C8h/3C9h, block set via int 10h AX=1017h, optional retrace wait when `v_5062`
    bit 800h is set;
  - dead code noted: `fn_FileSeekEnd` (0x08506) has no callers in this build;
  - style guide extended: `v_` anchor labels for unlabeled DGROUP structures/buffers (anchors are
    documentation only; numeric operands stay numeric) and a single top-of-file EQU block for
    layouts shared across segments;
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — M1 video boundary (annotated):
  - screen model: drawing goes through the segment variable `v_DrawSeg` (A000h = visible screen;
    the 64 KB `v_WorkSeg` buffer is the offscreen target). The main game has *no* present/blit
    wrapper — mode 13h writes go straight to video memory (only the palette writer waits for
    retrace);
  - mode seams tagged: int 10h AX=13h (cache init, return from the intro, restore) and AX=3
    (text mode on exit and on the error path); the intro effect uses VBE mode 101h (4F02h) and
    bank-switches window A with 4F05h (`sub_15F1F`) while decoding LOGO.PCX (`fn_15DC2`);
  - consequence for M1b: a mode-13h "present" boundary has to be *introduced* for streaming
    (frame_end after input/draw loop), unlike the palette which already has a funnel;
  - names: `v_50B8` -> `v_WorkSeg`, `v_50D2` -> `v_DrawSeg`;
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — M1b video/palette boundary spike (STREAM build + replay client):
  - all instrumentation is under `IFDEF STREAM` (`-DSTREAM`); the oracle build is untouched:
    `fn_StreamInit` allocates one 65,024-byte DOS block (64,000-byte shadow + message buffer +
    writer state) and creates `STREAM.BIN`; `fn_StreamPresent` is called from the two input-poll
    routines (`fn_PollInput`, `fn_WaitKey`), rate-limited to one frame per 2 BIOS ticks, only in mode
    13h, and emits one 1-line `rect` per changed row plus `frame_end`; palette updates are hooked
    at `fn_WriteDAC` (`DS:SI`), `sub_SetDACBlock` (`ES:DX`) and `sub_WriteDAC768`;
  - v0 VIDEO payload layouts pinned in `docs/protocol.md` (type byte + `set_mode`/`palette`/
    `rect`/`frame_end`/`reset`); the recorder writes a bare message stream (no session header),
    raw codec, 6-bit DAC values;
  - tools: `engine/asm/swreg/tools/build.sh --variant file` (variant build + alignment guard), `tools/dosbox/stream.sh`
    (headless run + replay), `tools/stream/replay.py` (PNG frames + final state), `docs/stream.md`;
  - gotcha: the 5-byte far-call hooks moved S1DEA's paragraph prefix from 7 to 5, so the game's
    hard-coded DS offsets (e.g. `EMMXXXX0` at `0CA0h`) no longer matched and the EMS init aborted
    with "Minimum of 1MB Expanded RAM must be available."; a 2-byte pad at the end of S1DC9
    restores prefix 7 and the build script now verifies prefix 7 + `EMMXXXX0` delta `0C99h`;
  - validation: 40 s no-input run -> 846 records (1 `set_mode`, 447 `palette`, 396 `rect`, 2
    `frame_end`), 479,755-byte recording; the client's SW Games logo frame matches the emulator
    snapshot from the same run; oracle build re-verified (`SWREG.EXE: OK`, `IDENTICAL`);
  - limits: VBE intro skipped, no input/audio/session header, palette messages not deduplicated,
    the stream build is slower than the original.
- 2026-10-19 — M1 input: keyboard wrappers (annotated):
  - `fn_PollInput` (0D0FEh) is the per-frame poll: DOS int 21h 06h, ASCII a-z folded to
    uppercase, AL=0 reads the extended scancode on a second 06h call; the event code shares
    `v_50CC` (0 none, 1 ASCII, 2 extended, 3/6 left press/hold, 7/0Ah right press/hold,
    5/9 releases) and the key/scancode lives in the renamed `v_KeyCode` (0B9Ah);
  - `fn_WaitKey` (06342h) blocks for a key or a mouse button, services sound via `fn_1888A`,
    maps left -> 0 and right -> ESC, clears the mouse latches and flushes the DOS typeahead;
    `sub_FlushKeyboard` (01D7Eh) is the 0Ch helper;
  - dispatch: `tbl_KeyDispatch` (081Ah, ASCII 20h..7Fh) and `tbl_ScanDispatch` (08DAh,
    scancodes 3Bh..76h) in `S0000`; the event-code branch is commented;
  - seam tags: `int 21h` 06h (poll key) and 0Ch (flush typeahead) marked INPUT; the stream
    hooks in `fn_PollInput`/`fn_WaitKey` keep their old positions;
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — M1 input: mouse wrappers (annotated):
  - S13A0: `fn_MouseInit` (13AEEh) probes the INT 33h vector, resets the driver, installs
    `fn_MouseHandler` (AX=000Ch, ES:DX = S13A0:0002, event mask 1Fh) and sets an 8:1
    mickey/pixel ratio; `fn_HideMouse`/`fn_ShowMouse`/`fn_RedrawMouse` wrap the visible bit;
    `fn_SetMouseCursor` (13B8Ah, 60 call sites) loads a 16x16 cursor image from
    PICTURES.VGA through the 9061h file struct into DS:3FE0h;
  - `fn_MouseHandler` (13A02h) is the user event callback (INT 33h 0Bh motion counters):
    motion updates the software cursor, button press/release latches the clamped position
    and sets `v_MouseState` bits; clicks land in `v_MouseLeft*`/`v_MouseRight*`, the release
    slots are written but never read in this build;
  - software cursor: `v_MouseX`/`v_MouseY` (0F10h/40F0h) is the drawn position,
    `v_MouseNewX`/`v_MouseNewY` (0F1Ch/0F1Eh) the target; the save/restore pair uses the
    16x16 buffer at DS:0E08h, `sub_MouseDraw` overlays DS:3FE0h with 0FFh transparency and
    clips the right edge; `fn_MouseMoveBy` clamps to the bounds and applies the 8-pixel
    hotspot for cursors other than 0/1Dh;
  - `v_MouseState` (40F6h) bit layout documented (present/visible/save guard/event latches/
    reentrancy; bits 1000h/0200h unknown); exit paths reset the driver with INT 33h 00h;
    every INT 33h call and the INT 21h 3533h vector probe carry MOUSE seam tags;
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — M1 sound: FM service wrappers (annotated):
  - S187E is the whole sound layer: `sub_SoundParseEnv` (18B83h) reads `SOUND=`/`BLASTER=`
    from the environment copy at DS:0AFF8h into `v_SoundPort`/`v_SoundIRQ`;
    `sub_SoundFindDriver` (18A35h) scans vectors 80h..BEh with DOS 35h for the driver's
    "FMDRV" signature at handler-segment:103h, patches the int number inside `sub_SoundCall`
    (18B80h, `int 0`) at CS:3A1h to the found vector, installs the status byte pointer
    (DS:0C2Ch) and allocates the music buffer; `sub_SoundPreInit` (18ACEh) loads the
    9Ch-paragraph driver block from WORLD.DAT and pre-inits through the far entry
    `v_SoundEntry` (1 base port, 2 IRQ, 3 detect, 4 enable, 5 status word at DS:0C2Eh);
  - `fn_PlayMusic`/`fn_MusicService`/`fn_MusicStop` queue tracks (`v_MusicId`; the 20-tick
    countdown v_50F4 is fed by the tick ISR), `fn_PlaySFX` plays WORLD.DAT effects through the
    far entry and falls back to `fn_SpeakerBeep` (PIT ch2 + port 61h) for effect 3 when SFX are
    off; `fn_SoundShutdown` releases the driver and all three buffers; `v_SoundState` (94D5h)
    bit layout documented (music/SFX present+enabled, 0040h no-signature, error flags);
  - driver command use documented from the call sites (int path 1/2/4/6/7/8/9, far path
    1..6/8/9); 0C2Ch/0C2Eh kept numeric: driver-written status words handed over by commands
    1/5; `v_94D1`/`v_94D3` left numeric (role not established);
  - open: `v_SoundEntry` (F7AEh) is zero in the image and has no writer in the game, yet the
    pre-init path runs by default - resolve with the driver black-box study (risk 1);
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — M1 tick: INT 1Ch wrappers (annotated):
  - `fn_TickInit` (0FE05h) reloads the four countdowns and points INT 1Ch at S0FBA:019Eh =
    `fn_TickISR` (0FD3Eh), saving the old vector at CS:0/CS:2 (DOS 351Ch); `fn_TickRestore`
    (0FE45h) restores it (251Ch) and runs from the error exit;
  - `fn_TickISR` documented: CS:6 counts ticks and folds them into `v_50F2`, CS:4 is the
    reentrancy guard (a reentered tick chains straight to the old handler), CS:8 the timeout
    counter armed by fn_0FE63, CS:0Ah/0Ch the saved SS:SP; the handler switches SS:SP to
    CS:19Eh, so the 399 filler bytes below the code are its stack, and it dispatches the
    countdowns v_50E8/v_50EA/v_50EC/v_50EE plus the music service countdown (v_50F4 ->
    v_5064 bit 10h);
  - direct BIOS tick reads tagged: the six `int 1Ah` waits in the attract/intro player
    (`fn_11B00`) and the STREAM build's frame pacing; `fn_DelayTicks` (084D4h) keeps its tags;
  - byte gate green: `SWREG.EXE: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — SBFMDRV.COM: byte-perfect disassembly + first analysis/black-box round:
  - `engine/asm/sbfmdrv/src/sbfmdrv.asm` reassembles to 7211 bytes, MD5 `0f679017d543b0947fc5a33456ba38a7`;
    `sh engine/asm/sbfmdrv/tools/verify.sh` prints `SBFMDRV.COM: OK` / `IDENTICAL` (0 warnings).  The
    generator/tooling (`engine/asm/sbfmdrv/tools/generate.py`, `build.sh`, `verify.sh`, `verify.py`) and the
    12 documented encoding fallbacks are checked in;
  - installer mapped: entry 100h -> 1975h; BLASTER parser 1C00h (only the A field; 220/240/260/280),
    DSP reset/detect 1907h/1A09h, FM detect 17ECh, free-vector scan 1A86h (80h..BFh), hooks
    INT 08h -> 1226h and INT 09h -> 12B2h, resident init 19AFh, `/U` uninstall 1B8Bh (signature
    scan 19DDh, MCB check 1A58h, `int NN; ret` gate 1B88h);
  - the installer checksum (182Fh: sum 11A7h bytes from 33Ah, stored at 100h) leaves `[101h]=0`;
    `sub_0C5E` then patches the immediate at `0DF3h` from 0 to 1, turning command 6's state write
    from `[02F2h]=0` into `=1` - that self-modification is what actually starts the sequencer;
  - resident INT API decoded (dispatcher 12E6h, tables 372h/392h): 0 version, 1 status-pointer
    exchange, 2 instruments, 3 timer reload, 4 PIT divisor, 5 scratch, 6 start stream,
    7 stop, 8 reset, 9 pause, A resume, B callback, C CS:032Ah, D position, E volume/tempo
    scaling, F scratch swap; -1 saved-vector slots, -2 CS:0149h, -3 CS:02F5h, -4 write slot,
    -5 CS:02F2h (music state).  Music state 0 idle / 1 playing / 2 paused; status byte FFh while
    active;
  - black-box harness (`engine/asm/sbfmdrv/test/`, DOSBox-X SB16 `A220 I7 D1 H5 T6`): TEST.COM finds vector
    80h, exchanges the status pointer (default CS:02F6h), exercises all commands, starts a zero
    stream (state 0 -> 1, status 00 -> FF, `[0DF3h]` 00 -> 01, position advances), checks the
    second-start `FFFEh`, pause/resume, and stop (state/status -> 0); the install-then-`/U` run
    reports `NO DRIVER`.  Results and interpretation: `docs/sbfmdrv.md`;
  - still open: full stream/OPL event decode (tables 33Ah/34Ah/352h), a real WORLD.DAT music
    block capture, and an audio-captured game run.  The game-side far entry is resolved:
    `v_SoundEntry` is zero (no relocation, no writer); a staged-game diagnostic shows the normal
    path reaches the first far call, which calls `0000:0000` (IVT bytes, not the INT 0 handler),
    and the driver has no far dispatcher - the resident INT API is the only interface.  The port
    should treat the far calls as no-ops.
- 2026-10-19 — SBFMDRV.COM: music stream format decoded and black-box traced:
  - the sequencer grammar is a MIDI-like running-status stream,
    `[status] payload... VLQ delay`: the status high nibble minus 8 selects one of the 8 main
    handlers and the low nibble is the channel; `8n` note-off, `9n` note-on (velocity 0 = off),
    `An`/`Dn`/`En` ignored, `Bn` controls `66h..69h` (status byte, rhythm mode, pitch bend), `Cn`
    program, `Fn` system (`F0` callback + jump, `F7` jump, `FC` stop, `FF 2Fh` stop + jump);
    the VLQ is big-endian 7-bit groups, the delay is in timer ticks and becomes the 32-bit
    countdown the ISR decrements (zero = keep processing in the same invocation);
  - voice engine documented: `0259h` states (active/released/free), allocation order, note map
    `039Ch`, one-octave F-number table `041Ch`, velocity curve `0A1Ch`, the 16-byte instrument
    records and their OPL register mapping, per-channel program/bend/gate tables;
  - `engine/asm/sbfmdrv/test/test.asm` now runs a crafted 41-byte stream through the real timer path and also
    steps the resident sequencer one event per call (temporary `retf` trampoline in the unused
    `02F9h` byte, restored immediately).  The trace matches the decode byte-exactly: positions
    `5,8,0C,0F,13,17,1B,1F,23,26,29`, status `5Ah` set/cleared, voice 0 note `3Ch` and voice 1
    note `40h` allocated and released (`80h`), F-numbers `1157h`/`11B0h`, self-stop via
    `FF 2Fh`, and command D returns the normalized stop snapshot (`0A06h:0001h`).  This corrects
    the first round's "position advanced" note: that wait was shorter than one PIT tick, the
    sequencer never ran, and `CDB` was the initial command 6 snapshot;
  - `[0151h]` is the sequencer-invocation counter (not PIT ticks); command 6 copies the start
    pointer to `012Dh/012Fh`, the track-repeat restart pointer used with command F's count;
  - docs: `docs/sbfmdrv.md` section 4 rewritten (grammar, event table, timing, voices, patches),
    section 6 replaced with the new trace; driver sources annotated; byte gate green:
    `SBFMDRV.COM: OK`, `IDENTICAL`, MD5 `0f679017d543b0947fc5a33456ba38a7`, 0 warnings;
  - still open: real `WORLD.DAT` music block + OPL register capture, percussion-path black-box,
    command 2 with a custom instrument table, audio-captured game run.

- 2026-10-19 — SBFMDRV.COM: real `WORLD.DAT` music blocks run through the reference driver:
  - `engine/asm/sbfmdrv/test/music.py` locates the game's record directory in DGROUP by content (dword
    offsets at `DS:0CED3h`, word sizes at `DS:0CF27h`, indexed by `id-1` from `fn_180F9`),
    extracts a record, walks its stream with the decoded grammar and cross-checks driver logs;
  - `engine/asm/sbfmdrv/test/music.asm` (`MUSIC.COM`) loads `MUSIC.BLK`, starts it with commands 2/4/6
    (tempo `1234DCh / word[+0Ch]`; `60h` -> 96 Hz) and single-steps the sequencer through the
    whole song, logging `S<step> P<position> X<status>`; `run.sh music` extracts, runs under
    DOSBox-X and compares (`MUSIC_TRACK`/`MUSIC_SONG` select the record / exact song slice);
  - the step loop masks IRQ0: DOS re-enables interrupts inside `INT 21h`, and with the timer
    live the resident ISR ran the sequencer behind the log (225 `[0151h]` invocations against
    222 steps).  With the mask the `[0151h]` delta equals the step count, which the check uses
    as a guard;
  - results: track 1 (328 events, 225 groups), track 18 (2041 events, 1436 groups) and track 8
    song slice (1214 events, 926 groups, includes 408 ignored `En` slots) all match the static
    walk exactly, `T = N`, self-stop via `FF 2Fh` and state 0;
  - `WORLD.DAT` layout documented in `docs/sbfmdrv.md` 4.1: CTMF header fields (magic, format
    `01 01`, instrument table at `28h`, stream after it, tempo `60h`, 16 per-channel flags,
    count at `24h`), AdLib-layout 16-byte patches, 21 CTMF blocks of which records 1..19 are the
    last 19, every song ends with `FF 2Fh` (no real stream uses `FC`/jumps), and the shipped
    size table is shifted (for ids 1..17 `size[id]` is the song length of record `id+2`, so ids
    2/4/7/8/12/14/15 load a window that cuts their song short; the game's id `14h` request is
    already past the table);
  - driver byte gate green: `SBFMDRV.COM: OK`, `IDENTICAL`, 0 warnings; game tree untouched;
  - still open: percussion-path black-box, custom instrument table via command 2, real
    `F0`/`F7`/`FC` streams (none shipped), audio-captured game run.
- 2026-10-19 — SBFMDRV.COM: percussion path black-boxed (`engine/asm/sbfmdrv/test/test.asm`):
  - the API run now ends with a second crafted stream: `B0 67 01`, one hit per drum (`9Bh..9Fh`
    on channels 0Bh..0Fh), one release per drum, `B0 67 00`, `FF 2F`; the trace logs the `BDh`
    shadow `[02F7h]` (`R`), the voice count `[014Bh]` (`C`) and the rhythm flag `[02F8h]` (`M`)
    after every step, then the drum note slots and the slot-6 frequency word.  IRQ0 is masked
    while stepping (same pitfall the music harness hit);
  - observed exactly the decoded behaviour: the mode switch gives 6 voices / BDh `E0h`, each
    hit sets its bit (`10h,08h,04h,02h,01h`), each release clears it, `B0 67 00` gives 9 voices /
    `C0h`, and `FF 2F` stops (state 0) with the silence path masking `[02F7h]` to `E0h`; the drum
    notes stay in slots 6..10 after the releases (a percussion note-off only clears the BDh bit);
  - docs 4.4 now spells out the three register writes per hit (TL = `40h` + drum level
    `10h,14h,12h,15h,11h`, +3 for the bass drum, and `A0h/B0h + {6,7,8,8,7}`) and that the drum
    operators' other registers come from the `sub_AF2` template loaded for voice slots 6..8;
    6.2 records the trace; the open-items list drops the percussion and custom-table entries
    (the real-block run installs each block's own table through command 2);
  - `CDB`/layout commentary in sbfmdrv.md 6 refreshed for the bigger test binary (the stream
    runtime offset moved from `099Bh` to `0A98h`);
  - driver byte gate green: `SBFMDRV.COM: OK`, `IDENTICAL`, 0 warnings.
- 2026-10-19 — SBFMDRV.COM: real songs played through the driver's own timer ISR, with audio capture:
  - `MUSIC.COM` gained a playback mode (command tail `P`, `MUSIC_PLAY=1`): instead of stepping, it
    leaves IRQ0 live, waits for the driver's self-stop and logs `PLAY ST=.. T=.. C=..` (final
    state, `[0151h]` invocation delta, elapsed BIOS ticks 0040:006Ch), bounded at ~60 s;
  - `music.py check --play` validates that line against the static walk: track 1 gives `ST=0000`,
    `T=00E1` (= exactly the 225 walk groups) and `C=00D5` = 213 BIOS ticks = 11.70 s against the
    decoded 1119 tempo ticks at 96 Hz = 11.66 s (0.4% off).  The timer path consumes the same
    groups with the same total delay as the static model -- the delay/tempo model is now verified
    on the real ISR, not only by manual stepping;
  - `MUSIC_CAP=1` wraps the run in DOSBox-X `DX-CAPTURE /A`; `music.py check --wav` reports the
    recording (track 1: 14.68 s, 48 kHz stereo, peak 41.8% FS, signal in 25/29 half-second
    windows), so the OPL output of a real song is audible and lasts as decoded.  This is the
    audio reference for the port's OPL layer and it regenerates headless (no GUI needed);
  - `run.sh` now patches the DOSBox-X profile per run (command tail, capture prefix, time limit:
    `MUSIC_PLAY`, `MUSIC_CAP`, `MUSIC_TIMEOUT`); step mode and its cross-check are unchanged;
  - docs: `docs/sbfmdrv.md` 6.1 records the playback/capture results; README and the script
    docstrings describe the modes.  Driver byte gate green (`SBFMDRV.COM: OK`, `IDENTICAL`).
- 2026-10-19 — audio and OPL-register capture in the oracle harness (both gates green):
  - found two DOSBox-X capabilities the earlier driver work had written off: `DX-CAPTURE /A|/O`
    (start a mixed-audio WAV capture, or an OPL register log in DRO 2.0 format, around a program)
    and the fact that both can run at once;
  - `tools/dosbox/audio.sh [seconds]` (`AUDIO_MODE=wav|dro|both`) runs the original `CH2.BAT` with
    `DX-CAPTURE /A /O SWREG.EXE ... /W /B` patched into a copy of the batch.  Two traps found by
    experiment: wrapping `CH2.BAT` itself stops the capture at SBFMDRV's TSR install, and starting
    `SWREG.EXE` outside the batch makes the game print "Please run Yendorian Tales Chapter 2 from
    CH2.BAT" (it requires the `/B` switch the batch passes);
  - observed 40 s run: `swreg_000.wav` 39.97 s, peak 41.8% FS, 79/79 half-second windows with
    signal; `swreg_000.dro` 1379 writes, 101 registers, 267 note-on writes; every one of the 266
    note-on gaps lies on the 10.42 ms tick grid of the shipped blocks (96 Hz) -- the game really
    drives the resident music engine itself, and the port now has an audio *and* a register-level
    reference for its OPL layer;
  - `tools/stream/dro.py` decodes DRO 2.0 (header, codemap, short/long delay codes, initial register
    snapshot) with `summary` and `dump` modes; `tools/stream/wav.py` reports capture statistics and
    fails on silence;
  - `engine/asm/sbfmdrv/test/run.sh music` gained `MUSIC_CAP=wav|dro|both` (one `DX-CAPTURE /A /O` run) and
    `music.py check --dro` compares a playback run's register log with the static walk: track 1
    gives 808 writes, **161 key-ons and 161 key-offs, exactly the walk's note-on/note-off counts**,
    spanning 11 656 ms, with every key-on within one tick (10.1 ms) of the decoded timeline;
  - docs: `docs/oracle.md` gained the capture section + the DRO format notes; `docs/sbfmdrv.md`
    6.1/8 updated (the game-level audio item is done; the drum-TL comparison and crafted jump
    streams stay open); `CONTRIBUTING.md` and `engine/asm/sbfmdrv/README.md` list the new scripts/flags.
- 2026-10-19 — SBFMDRV.COM: percussion OPL replay and the jump/callback/FC paths (both gates green):
  - `API_CAP=dro` wraps TEST.COM in `DX-CAPTURE /O`; the capture does not change the run
    (RESULT.TXT byte-identical with and without it).  `engine/asm/sbfmdrv/test/perc.py` replays the
    percussion section against the capture: 26 writes from `BD=E0h` (rhythm on) to `BD=C0h`
    (rhythm off), TL + A0h + B0h + BDh per hit and BDh per release, all matching the decoded
    path -- the TL curve is pinned to the reference velocity table at CS:0A1Ch, indexed
    `(velocity+4)>>3` (verified against the image).  Docs 4.4 corrected: the note-on also
    writes `BDh` (that is the drum key-on), the `B0h` write has no key-on bit, and a
    percussion note-off writes `BDh` only;
  - found a capture quirk worth knowing: DOSBox-X's DRO writer keeps a **last-value cache per
    register** and drops writes that do not change a value (both captures have zero such
    writes), which is why the cymbal's `B0h=09h` is absent -- the replay applies the same
    filter;
  - three crafted streams cover the control events and API command 0Bh: F7 forward jump over
    traps (+8 -> `P000C`), F7 backward jump looping on the stream's first delay byte
    (`P0005` on every step), FC stopping without a delay read (`P0011`, state 0), and F0
    calling the command 0Bh callback exactly once (`F0CB N0001`) before its +4 jump;
  - docs 4.2 gained the jump/FC/callback semantics (offset relative to just after the offset
    VLQ, applied as 32-bit two's complement; a target is a *delay byte* because every
    non-stop event reads a delay), 6.2 the OPL replay, 6.3 the traces; the open-items list
    drops the drum-TL and F0/F7/FC entries (crafted-only coverage is stated);
  - gates: `SBFMDRV.COM: OK` / `IDENTICAL`, `SWREG.EXE: IDENTICAL`.
- 2026-10-20 — seam split + live client link (the first interactive test path):
  - **split**: all integration code moved out of the game source into `engine/asm/swreg/src/seam.asm` (included
    under `IFDEF SEAM`); `swreg.asm` keeps six tagged hook sites — start-up init, the present hook
    in each of the two input-poll wrappers, and the three DAC hooks — plus the include. The 5-byte
    far-call prefix shifts S1DEA's paragraph prefix, so the 2-byte pad that restores prefix 7
    moved into the seam file with the state it protects;
  - **pin**: `engine/asm/swreg/tools/seam_pin.py` strips the `IFDEF SEAM`/`IFDEF STREAM` blocks out of
    `swreg.asm` and hashes the rest; the baseline was pinned *before* the refactor
    (`sha256 3435777…871e5`, 1,051,189 bytes / 46,160 stripped lines) into `engine/asm/swreg/src/swreg.pin`,
    and `verify.sh` checks it first, so an accidental edit to game code fails the gate. `--check`
    is green after the refactor: only seam blocks changed;
  - **palette policy**: the DAC hooks now only copy into a pending 768-byte palette; a send is the
    diff span against the palette the client already has, rate-limited to one send per 2 BIOS
    ticks from the hooks and flushed unconditionally once per present pass; every send pass ends
    with `frame_end`, so a palette-only pass still presents and the last step of a fade is never
    stranded. A received `'K'` (0x4B) requests a keyframe (`set_mode` + full palette + full-frame
    pass), which makes client/server start order irrelevant;
  - **builds**: `build.sh --variant serial` (`-DSEAM`, COM1 115200 8N1), `--variant file`
    (`-DSEAM -DSEAM_FILE`, raw `STREAM.BIN`), `--variant tee` (`+ -DSEAM_TEE`, file *and* serial in
    one run — the file stream is the ground truth for link checks), `--variant log`
    (`+ -DSEAM_LOG`, `SEAMLOG.TXT` packed records); all run `fixhdr.py` + `check_mz_layout.py`;
  - **client**: `client/` is C# (`net11.0`, SDL3 via P/Invoke, no NuGet) — reader thread, display
    state, resync on the magic, keyframe request on connect, stats, `--ppm`/`--dump`/`--headless`;
    `client/server.sh` stages the seam build and runs the game window, `tools/dosbox/dosbox-x-client.conf`
    bridges COM1 to TCP;
  - **link facts** (each cost real time): `serial1 = nullmodem port:2324` — server is the default,
    while `server:<host>` means *client* mode and fails silently; `multiplier:16` scales the
    emulated baud (~165 KB/s measured; `1` -> 11,465 B/s = 115200/10, `4` -> 46,004 B/s), which
    keeps a ~70 KB keyframe under half a second; and the nullmodem **doubles every `0xFF` byte**
    on the wire (telnet escape, `CNullModem::transmitByte`), undone client-side by
    `NullModemStream`.  It is deliberate and switchable: `transparent:1` (the value must be
    numeric — `transparent:true` is *silently ignored*) turns escaping and telnet handling off,
    which makes the link raw and needs the client's `--raw`; both combinations verified
    byte-exact against the tee build's `STREAM.BIN`;
  - **verification**: file backend 45 s no-input -> 482 messages / 150,316 bytes (42 frames, 402
    rects, 38 palette messages); live link with the tee build -> the client received 478 messages
    / 149,482 bytes and `tools/stream/compare_streams.py` reports them **byte-identical** to the seam's
    own `STREAM.BIN` (the four missing ones predate its connection), with the rendered final frame
    pixel-identical (0 differing index/palette bytes) to an independent decode of that stream; a
    replayed recording renders byte-identically to the Python decoder's PPM; A/B against the
    pre-refactor stream build is pixel-identical on final frames (mid-fade differs only by the
    2-tick pacing);
  - scope note: this client is a **development harness for the boundary**, not the M8/M9
    deliverable — VIDEO only, no input/audio/VBE, no session headers;
  - gates: `SWREG.EXE: OK` + `IDENTICAL` (default build), pin check green, seam builds assemble
    with 0 warnings and pass the layout check, `dotnet build client -c Release` clean.
- 2026-10-20 — repository layout settled, gates committed, Windows bundle reproducible:
  - **tree**: `engine/asm/swreg/` (was `asm/`), `engine/asm/sbfmdrv/` (was `driver/`),
    `engine/c/` (placeholder), `client/`, `tools/{dosbox,stream,repro}/`, `tests/{replay,link,data}/`;
    `docs/layout.md` documents the tree, the `build/` mirror and where a given change goes.  The
    harness split by purpose: `tools/dosbox/` (profiles, runners, X helpers, the client's nullmodem
    profile) and `tools/stream/` (`replay.py`, formerly `stream_client.py`, plus
    `compare_streams.py`, `dro.py`, `wav.py`);
  - **build**: the four single-purpose seam scripts collapsed into
    `engine/asm/swreg/tools/build.sh --variant oracle|serial|file|tee|log` (`build.bat` takes the
    variant too); outputs are `build/swreg/<variant>/`.  The client's MSBuild output moved to
    `build/client/{bin,obj}` via `client/Directory.Build.props`, and the driver's to
    `build/sbfmdrv/`; run artifacts are `build/runs/<kind>/<stamp>/` for every kind
    (oracle -- audio runs as `oracle/<stamp>-audio` -- stream, link, replay, repro, sbfmdrv,
    server);
  - **committed gates**: `tests/replay/run.sh` (the client's render must equal the Python
    decoder's, `cmp` on the PPM; hermetic, uses `tests/data/sample-video.bin`) and
    `tests/link/run.sh` (tee build -> DOSBox-X -> client, verified byte-exact against the seam's
    own `STREAM.BIN`); `tools/repro/nullmodem-0xff/` records the 0xFF finding with measured
    numbers (224 discontinuities per 57,599 bytes by default, 0 with `transparent:1`);
  - **Windows bundle**: `tools/publish-win.sh` publishes a self-contained win-x64 single-file
    `Yt2Client.exe`, pins SDL3 3.4.18 win32-x64 (archive *and* DLL sha256 verified, `--sdl3` for a
    local DLL), stages the game side (`server/game/<variant>`, `server/dosbox-x.conf`,
    `start-server.bat`), the sample recording and `smoke-test.bat`, and zips it to
    `build/dist/yt2-win-x64.zip` (~38 MB); the bundle cannot be run where it
    is built (no Windows/wine here); its smoke test is the first thing to run
    on the target;
  - gates re-run green after every move: default build `SWREG.EXE: OK` + `IDENTICAL` + pin OK,
    `SBFMDRV.COM: IDENTICAL`, all seam variants assemble with 0 warnings and pass the layout check,
    replay and live link byte-exact, and the serial/file seam images unchanged by the reorg
    (`3028ec99…`, `2409adb2…`).
- 2026-10-20 — the link becomes interactive (CONTROL + keyboard INPUT):
  - **framed RX**: the seam parses protocol-v0 messages from COM1 (`sub_SeamRxByte` /
    `sub_SeamDispatch`), staging partial messages across polls; a bad header drops the staged bytes,
    an unknown family answers one `error`, and the old bare `'K'` keyframe byte still works;
  - **CONTROL**: the engine sends `hello(role 0, version 0)` before its first `set_mode` and answers
    a client `hello` once per session; `keyframe_request` replaces `'K'` for the client;
  - **keyboard**: INPUT `key(down, scan, ascii)` is written into the BIOS keyboard ring at
    `0040:001A` with interrupts held off, which is exactly what the game's DOS `06h`/`0Ch` calls
    drain — no game code and no new hook site.  Releases are ignored (the ring is type-ahead only);
  - **client**: `FrameWriter` (framed TX, per-direction sequence), the hello/keyframe handshake
    retried once a second until the engine answers (DOSBox-X drops host bytes before the game
    reaches the seam), SDL key events mapped to XT scancodes + ASCII (`client/DosKeys.cs`),
    `--key SECONDS:SCAN:ASCII` as the scripted-input hook, `bye` on exit, engine notices and
    `control=`/`engine_errors=` in the stats line.  Esc is forwarded to the game; the client quits on
    window close or Ctrl+Q;
  - **bugs found while wiring it** (each cost a run): the RX poll was skipped whenever the backend was
    the file backend, so the *tee* build never read the link at all; three `seam_txfam` writes used
    the block segment instead of `cs:`; and the parser checked the magic byte-swapped
    (`59 54` instead of the little-endian `54 59` that `0x5954` puts on the wire — now called out in
    `docs/protocol.md`);
  - **verification**: link test with a scripted Return at 20 s — 690 messages / 220,724 bytes sent,
    the client received 685 / 219,873 (5 predate its connection) and every message is byte-identical
    to the seam's `STREAM.BIN`; the engine hello arrives and is printed; the key changes the game
    (the stream jumps from 482 to 688 messages), taking the logo to the next screen, which happens to
    be VBE and therefore black client-side until VBE streaming lands.
- 2026-10-20 — VBE intro streaming (both gates green):
  - **engine** (`fn_SeamPresent` and friends in `engine/asm/swreg/src/seam.asm`): the present path now
    detects the mode each pass (`int 10h 0Fh`, then `4F03h` = 101h for the intro's 640x480x8) and
    keeps a small mode state (`seam_vmode`/`seam_vbank`/`seam_vbudget`).  Mode switches send
    `set_mode` (640x480x8 on entry, 320x200x8 on the way back) and rely on the documented "a
    `set_mode` means start from a blank screen" rule instead of a full repaint; a keyframe request
    now only sets the flags and forces the mode message, so it works in both modes;
  - **no second framebuffer**: 640x480 = 307,200 bytes cannot be shadowed in the seam block, so each
    row is read through the VBE window (`4F05h` window A, bank reprogrammed only on a change, rows
    that cross a 64 KB boundary read in two chunks) and compared with a 32-bit checksum per row; a
    changed row is sent as `rect(0,y,640,1)`.  The checksum table lives at the mode-13h shadow area,
    which is unused in VBE.  A pass carries at most 16 KB of rows (~25 rows), so a full repaint is
    spread over passes and never stalls the game;
  - **client**: `set_mode` reallocates the replica and clears it (`ClientStats.ModeChanges`), and
    `SdlScreen.Resize` rebuilds the texture and the integer-scaling hint, so the window survives the
    intro -> game transition.  `tools/stream/replay.py` carries the size as state as well;
  - **verification**: the byte gate stayed green (`SWREG.EXE: OK`, `IDENTICAL`, 0 warnings) and the
    default build contains none of this code; a 20 s file-backend run recorded
    `modes 320x200 -> 640x480 -> 320x200`, 873 records / 341,240 bytes, and two runs are
    byte-identical; the client and the Python decoder agree byte-for-byte on the new VBE fixture
    (`tests/data/sample-vbe.bin`, 200 records / 123,257 bytes, 11 frames at 640x480, 184 rows) as
    well as on the old 13h fixture, and `tests/replay/run.sh` runs both by default; the live link
    (tee build) sent 1072 messages / 464,680 bytes of which the client received 874 / 341,697
    byte-identical after joining at message 198; the rendered intro was checked visually against the
    emulator (the X-window snapshot goes black during VESA — a DOSBox-X capture limitation
    documented in `docs/oracle.md`, not a client fault);
  - **bug found**: the cached VBE bank cannot be trusted across passes (the game draws through the
    same window in between), so it is invalidated at the start of every pass.
- 2026-10-20 — mouse input (INT 33h shim; transport limits measured):
  - **game side**: `fn_MouseInit` probes `int 21h 3533h`, resets with `00h`, installs
    `fn_MouseHandler` with `0Ch` (mask 1Fh) and asks for an 8:1 ratio with `0Fh`; its handler then
    only reads the `0Bh` motion counters.  `sub_SeamMouseInstall` (called from `fn_SeamInit`, i.e.
    before the probe) saves the vector, forwards everything the game uses and chains the rest to
    DOSBox-X's driver.  `mouse_move`/`mouse_button` update shim counters and position, then
    `sub_SeamMouseEvent` calls the game's handler exactly as a driver would (AX mask, BX buttons,
    CX/DX position), so the game's software cursor follows the client;
  - **bug found and fixed**: the first version restored AX/BX/CX/DX in the handler's epilogue, which
    made `int 33h 00h` return 0 instead of FFFFh — the game then concluded "no driver" and never
    installed its handler (visible in the log as `0Ch` never happening).  INT 33h returns values in
    those four registers; only the rest is preserved;
  - **link work**: the present pass now polls the UART inside the row scans (every 64 rows), and the
    transmit wait loop parks arriving bytes in a 32-byte ring, because the tee mirror saturates COM1
    and a full RX FIFO drops host bytes — the reason single-shot input could vanish.  The VBE scan
    became a rolling 160-row window so a pass stays short; a whole 640x480 screen is scanned every
    three passes;
  - **client**: `--mouse S:DX:DY` and `--click S:BUTTON:0|1` scripted hooks, relative mouse mode
    (`SDL_SetWindowRelativeMouseMode`), event pump for SDL3 mouse motion/buttons (offsets documented
    in `client/Sdl.cs`), host pixels divided by the integer scale with the remainder kept, and
    interactive keyboard forwarding wired into the same pump (it had only been scripted before);
  - **verification**: `SWREG.EXE: OK`, `IDENTICAL`, `seam_pin: OK`, the serial and file builds pass
    `check_mz_layout.py`, `tests/replay/run.sh` is green on both fixtures and `tests/link/run.sh`
    (30 s, scripted mouse and click) reports every received message byte-identical.  Engine-side the
    shim is proven end to end in the log build: `00h` answered, `0Ch` installs at S13A0:0002 with
    mask 1Fh, and mouse messages arrive and dispatch (`tag 8`, family 3 type 1/2).  What is *not*
    verified live is the game's reaction: this harness's nullmodem drops host-to-guest bytes when
    the game is not reading, so a scripted click after the intro starts is usually lost before the
    UART sees it (keyboard behaves the same, and the handshake only survives because it retries).
    The `--mouse`/`--click` hooks are in place for a real trial; expect input to work while the game
    is polling and to be flaky during long non-polling stretches of this emulator profile.
- 2026-10-20 — client window (done):
  - **symptom**: the Windows bundle printed `client: window 960x600 (windows / direct3d11)` and
    stayed console-only.  SDL reported success because it had done what it was told;
  - **cause**: `client/Sdl.cs` passed `0x8` as "resizable".  In SDL3 `0x8` is `SDL_WINDOW_HIDDEN`
    and `SDL_WINDOW_RESIZABLE` is `0x20`, so the window was created and never mapped (not in the
    taskbar either).  D3D11 renderer creation and every frame upload succeeded, which is why the
    replay, link and stream gates all stayed green: they render offscreen.  The same mistake also
    set `SDL_INIT_EVENTS` to 0x40 (joystick) instead of 0x4000, harmless only because video implies
    events;
  - **fix**: correct flags, centre the window, call `SDL_ShowWindow` when the window system still
    reports it hidden (and say so), and print the real geometry and backend:
    `client: window 960x600 at 32,84 (Windows / Direct3D 11)` (SDL names are mapped to product
    spelling);
  - **related defects found while verifying**: the window was created *after* the TCP connect, so a
    failed connect left a console-only client (`tests/link` starts it late on purpose); `--hold` was
    parsed but never implemented, so `smoke-test.bat` would flash and exit; redirected stdout was
    block-buffered, hiding the startup lines of a running client.  The window is now opened first,
    `--hold` pumps until the window is closed, and both console streams are line-live;
  - **new gate**: `tests/window/run.sh` runs the client on a private Xvfb display and requires a
    mapped window (`Map State: IsViewable`), 960x600, and the first presented frame.  Proven by
    reintroducing the old flags: the gate fails with `IsUnMapped` while `tests/replay/run.sh` stays
    green;
  - **verification**: asm untouched (`SWREG.EXE: OK` / `IDENTICAL` unaffected); `tests/replay/run.sh`
    green on both fixtures; `tests/link/run.sh` (25 s, tee build) received 678 messages /
    218,546 bytes, every message byte-identical to `STREAM.BIN`; the window gate is green with the
    fix and red with the old flags.  The Windows `.exe` itself still needs the user's machine.
- 2026-10-20 — late client resync + VBE 0x100 (done, stall open):
  - **symptom**: a client that connected while the VBE logo was playing stayed at 320x200 and
    printed one `bad rect ... (resyncing)` per 640-wide row for the rest of the session, so the
    window showed garbage.  Reproduced locally by starting the client ~1 s after DOSBox (it joins
    mid-intro): 44 bad rects, no `set_mode(640,480)` on the wire after the join;
  - **cause**: the handshake burst is `hello` (17 bytes) + `keyframe_request` (15) = 32 bytes and
    the engine's UART FIFO holds 16.  The `hello` survived, the keyframe behind it was dropped, and
    the client stopped retrying as soon as *any* record arrived, so the engine never learned that a
    late client needed a keyframe.  The engine only volunteers `set_mode` when the game changes
    mode, hence the wrong mode forever;
  - **fix (client)**: retry the keyframe until a `set_mode` arrives (and `hello` until the engine's
    hello), keyframe frame first so it is the one that fits the FIFO; a well-framed `rect` that does
    not fit the current mode is no longer a framing error - it bumps `bad_rects`, requests a
    keyframe (rate-limited) and leaves the stream aligned;
  - **fix (engine)**: the seam streamed only VBE 101h (640x480, the logo).  The title and later
    screens are 100h (640x400x8), so the client went black there.  `seam_vh` carries the height,
    both modes are accepted, and a height change clears the checksum table and sends a fresh
    `set_mode`; `sub_SeamEnterVbe` and the row wrap use it.  `fixhdr.py` now also pins
    `eMinAlloc` to 33: UASM writes 0 when the stack segment is not last, the reference asks for 528
    bytes and DOSBox aborts with `Corrupt MCB chain` without them;
  - **verification**: early-join link run (tee build, client 1 s after DOSBox) - 0 bad rects, the
    engine re-sent `set_mode(640,480)` at the join point (message 189 of 188+), the client followed
    the logo back to 320x200, and `compare_streams.py` reports every received message byte-identical.
    `verify.sh`: `SWREG.EXE: OK`, `IDENTICAL`; `seam_pin: OK`; layout checks pass for serial, file,
    tee and log; replay and window gates green; the default link gate is byte-identical;
  - **open**: every seam build stops at a black screen right after the publisher logo and never
    reaches the 640x400 title, while the unmodified game (same profile, same data files, same Xvfb
    harness) shows it at 15-35 s.  Ruled out: the seam's runtime (a build whose `fn_SeamInit` and
    hooks return immediately stalls the same way), the 66 KB conventional block (a TSR keeping the
    same 4,160 paragraphs in front of the original game is harmless), and the EXE's file size
    (appending 3,369 bytes to the original changes nothing).  What is left is the image layout:
    the seam segment sits between `S1DEA` and the stack, which moves `SS` from 0x2d66 to 0x2e38.
    Moving the include after `STK` keeps `SS` but makes DOSBox abort at load with
    `Corrupt MCB chain` (the seam then has no `STREAM.BIN` either), so that placement is not usable
    as is.  Next: find which segment the game depends on and place the seam
    code/data where it keeps every game segment address, or prove the
    dependency is not layout at all (both suspects were tested and ruled out;
    see the final entry).
- 2026-10-20 — window live while connecting and the stall narrowed (open):
  - **symptom**: the client window was frozen ("not responding") until the
    server answered.  `TcpSource.Connect` ran on the main thread after the
    window was created and nothing pumped SDL for up to `--connect-timeout`;
  - **fix (client)**: the connect loop pumps the window between retries, a
    black 320x200 frame is presented before connecting, and closing the window
    cancels the wait quietly.  The relative mouse capture follows focus
    (losing the window releases the pointer) and `Ctrl+M` toggles it;
  - **verification**: with no listener the window is mapped at 960x600 and
    `Ctrl+Q` during the retry loop exits immediately; window and replay gates
    green (docs/client.md);
  - **stall, corrections**: `SS` is not the trigger - the oracle EXE with
    `e_ss` raised by one paragraph still shows the castle at 55 s.  Nor is
    image growth - an oracle build with an extra 3,300-byte dummy segment
    before `STK` (same `eMinAlloc` pin, no seam code) also shows the castle.
    The seam segment plus its hook calls add 32 bytes (the pad at the end of
    `seam.asm` keeps the data segment at prefix 7); deleting the calls breaks
    that alignment and the game exits at startup, so the calls have to stay;
  - **next probe**: log the caller's `CS:IP` at the present hook in the log
    build, so the stalled game's spin location is visible instead of inferred.
- 2026-10-20 — client survives the server; transport ceiling measured:
  - **reconnect**: a lost link used to end the client - the reader cancelled
    the whole process, and a write racing the dead socket escaped as an
    IOException.  The window now stays, prints `link lost; waiting for the
    server` and reconnects; each session gets a fresh state and handshake, the
    mouse is released while waiting, and writes on a dead socket are swallowed
    and marked.  Verified by killing DOSBox mid-run and restarting it: two
    sessions, frames after the second, client alive throughout; a TCP reset
    with no server is survived the same way.  Headless runs still end with the
    link, so the gates stay short;
  - **transport ceiling**: a saturating COM1 writer (poll THRE, divisor 1)
    measures 180 KiB/s at multiplier 16, 139 KiB/s at 64, 152 KiB/s at 1024,
    and 161 KiB/s at 100 MHz - the nullmodem path is per-byte host cost, not
    baud.  The emulated UART cannot reach MB/s, and the guest poll loop cannot
    outrun it;
  - **shared directory**: the same writer doing 32 KiB INT 21h writes into the
    mounted directory reaches 7.8 MiB/s at stock cycles.  A file-backed video
    link (client tails the growing `STREAM.BIN`) is the way to MB/s; input can
    stay on the UART, which carries a few hundred bytes per second.
- 2026-10-20 — folder link, and the stall explained:
  - **folder transport**: files per direction, as the link the bundle uses.
    The engine appends to `STREAM.BIN` (file backend) and reads `INPUT.BIN`
    from the present poll; the client gains `--follow PATH` (blocks at EOF,
    rewinds when the file shrinks) and `--input PATH` (appends framed input,
    flushes per message).  Both files are pseudo-rings: at 8 MiB / 64 KiB the
    writer truncates to zero and restarts, the reader rewinds, and a video
    wrap is followed by a forced keyframe.  The engine seeks to its read
    position before every `INPUT.BIN` chunk because DOS keeps EOF set on a
    file that grows under it.  Verified live: a client started after the game
    receives the debug history, the engine reads the file (RX dispatches in
    the log build for keyframe_request/hello/keys) and answers with a fresh
    `set_mode`; a 256 KiB scratch limit wrapped once, the client rewound and
    kept rendering without decode errors.  Gates after the change: oracle
    `IDENTICAL`, replay, window (mapped 960x600) and link (878 messages,
    byte-identical) all green; the Windows bundle now stages the `file` build
    and `run-client.bat` passes `--follow/--input`;
  - **palette, investigated to the source**: the client renderer itself is
    exact - a synthetic stream of known RGB values renders the same colours in
    the window and in the PPM (the earlier "palette" suspicion there is
    closed).  The colours the user sees are the last palette the engine sent
    before its output stops: the client freezes on a mid-transition frame, so
    the symptom is the stall, not the DAC capture;
  - **stall root cause**: the six 5-byte hook calls inserted into game code
    shift every following code offset.  An oracle build with only those same
    5-byte NOPs (no seam code at runtime, same layout shift) stalls at exactly
    the same point, while the unmodified oracle reaches the castle; the
    data-segment pad keeps `S1DEA` at prefix 7 but cannot undo the code
    shifts.  Next: replace the inserted calls with size-preserving hooks
    (a call over the first five bytes of the target function, with the seam
    executing the displaced instructions and jumping back to +5), so the
    game's code offsets stay put.
- 2026-10-20 — launchers, pointer policy, client-first start order:
  - **launchers**: the folder-link bundle shipped LF-only `.bat` files (an
    editor rewrite), and cmd.exe mis-executes the parenthesised blocks in
    `start-server.bat`, so DOSBox-X never started.  CRLF is restored and
    `publish-win.sh` rewrites every staged `.bat` to CRLF, so an LF-writing
    editor cannot break a bundle again;
  - **pointer**: the client no longer uses SDL relative mouse mode and never
    hides the cursor.  It turns the pointer's absolute position over the
    focused window into game-relative motion (scale-divided, remainder kept)
    and forgets that position on leave/focus loss, so the desktop keeps the
    pointer outside the window.  `Ctrl+M` is gone; `Ctrl+Q` stays;
  - **start order**: the client no longer creates `STREAM.BIN`; it opens an
    existing file, prints `follow: waiting for ...`, and retries on a sharing
    violation.  The engine owns the file, so a client started first cannot
    hold a handle DOSBox has to create.  Verified client-first end to end: no
    file at launch, engine created it, client decoded 401 KB, and the game
    still reaches mode 13h.  Replay, window and link gates green.
- 2026-10-20 — size-preserving hooks, stall fixed, palette read from the DAC:
  - **hooks**: all six seam sites (start, `fn_WaitKey`, `fn_PollInput`,
    `fn_WriteDAC`, `sub_SetDACBlock`, `sub_WriteDAC768`) now replace whole
    instructions of the same length with a far call to a seam tail that runs
    the displaced instructions and rejoins; no byte is inserted, so the
    game's code offsets (and the raw data offsets `4124h`/`4424h`/`4D56h`)
    stay exactly where the reference puts them.  The layout gate reports
    `S1DEA prefix 7, EMMXXXX0 delta 0xC99`, the oracle stays `IDENTICAL`,
    and the pin is `7c69d26f08528d41ce3e9da49ce85949b8fe543ef25edca9b3c1b28e09d5b6c3`;
  - **stall**: with the offsets intact the file build reaches the castle at
    about 40 s and the narration at 60 s (proof frames in
    `build/runs/stall/file-20261020-161736/`).  The earlier oracle-with-NOPs
    experiment had isolated the inserted calls as the cause; this confirms
    the fix;
  - **stale input**: the engine seeks `INPUT.BIN` to its current end on the
    first open, so a server-only start cannot replay a previous client
    session's keys/clicks into the fresh game; a client that starts first
    still retries its hello/keyframe request;
  - **palette**: the capture no longer trusts the game's palette source
    pointers.  The fade tables are zero for many writes and the block/768
    paths missed the scene's low entries, so the client kept the intro
    palette; reading the DAC back through ports 3C7h/3C9h at the end of each
    present pass and diffing against the copy the client has captures every
    writer.  The old sent-palette update also wrote through `SS:BP` (a
    missing `ds:` override), so diffs were re-sent forever; both are fixed.
    The client's frame is now pixel-identical to the DOSBox window at the
    main menu (64000/64000), and the castle/narration frame matches
    visually; palette messages reuse the palette span message, so the
    protocol and the client stay unchanged.
- 2026-10-20 — frame rate, absolute mouse, and the pale shadow:
  - **frame rate**: the file backend used to pay one `INT 21h` write per row, which on a host
    directory is slow enough to cap a pass at a few milliseconds per row.  Messages now go into a
    16 KiB `SEAM_TX` buffer in the seam block and one write flushes a whole present.  The block's
    old 64 KB mode-13h shadow was replaced by 4-byte per-row checksums, which frees the space the
    buffer needs and removes a real wrap bug: the shadow started at `0940h`, so its last two rows
    (offsets `0FC0h`+) wrapped past `0FFFFh` into `0000h`/`0300h` — the DAC buffer and the first
    entries of the sent-palette copy that the client-diff is computed against.  A changed bottom
    row could therefore poison the diff, which is the "subtle" palette mismatch: reads and writes
    of the sent copy disagreed.  The checksum pass rescans all 200 rows every present and sends
    rows whose checksum changed (no shadow at all);
  - **delivery**: the engine commits `STREAM.BIN` after every flush (`INT 21h` AH=68h).  Without
    it DOSBox keeps the host-side buffer, so the reader receives the stream in bursts and the last
    frame of each burst is never seen; the bundle test went from a stale frame to pixel-identical;
  - **palette**: a present pass now always reads the DAC and sends the diff (the `seam_flushpal`
    flag is gone).  The game also sets palettes through paths no hook sees (BIOS block loads, the
    menu fade), so gating the readback on the hooks' dirty bit left the client on a stale palette
    until the next hooked write.  Palette hooks only mark the palette dirty for writes between two
    passes; the two-tick pacing keeps the readback off the game's own DAC writes;
  - **mouse**: the client no longer sends deltas.  SDL pointer positions are divided by the window
    scale and sent as `mouse_abs(x, y)` (INPUT type 3, `u16 x`, `u16 y`) in game pixels.  The
    engine's shim sets its motion counters to `target - current` and calls the game's `0Ch`
    handler, so the game's own `fn_MouseMoveBy` clamps and lands exactly on the target: the
    mapping is proportional (window upper-left to game upper-left, centre to centre) in both
    320x200 and 640x480, and repeated updates cannot drift.  `mouse_move` stays for recordings;
  - **per-frame hook**: `fn_DelayTicks` gets the same size-preserving present hook as
    `fn_PollInput`/`fn_WaitKey`; animations that pace with delays now stream without waiting for
    the next input poll;
  - **Linux bundle**: `tools/publish-linux.sh` + `client/publish/linux/` publish a self-contained
    linux-x64 single-file client with `libSDL3.so.0`, the file-backend server, `start-server.sh`,
    `run-client.sh`, `play.sh` and a smoke test — the same shape as the Windows bundle;
  - **verified**: oracle `SWREG.EXE: OK` + `IDENTICAL`; layout `S1DEA prefix 7, EMMXXXX0 delta
    0xC99`; seam pin refreshed to `7e890a07b9766b5d`; replay/link/window gates green; the
    published Linux bundle starts the server and client and the client's last frame is
    pixel-identical to the DOSBox-X window at the menu (64000/64000); a host pointer moved to the
    client window's centre puts the game's cursor at (160,100).
  - **smoothness follow-up**: the transport was not the limit — with the host pointer moving across
    the menu the client received 9.1 frames/s, exactly the two-tick pacing, and each 200-row pass
    finished within one BIOS tick.  A pass now runs once per tick (18.2 Hz) and the palette
    readback only runs when a palette hook fired, plus a once-a-second resync; with the same
    pointer test the client now receives ~18 frames/s.  The VBE pass moved from 16 KB to 32 KB of
    rows per visit so a full 640x480 keyframe takes about ten passes.  A mode set blanks the DAC,
    so the next four passes skip the palette readback (`seam_palhold`) — that blank read was the
    one-frame black flash the player saw; the menu frame is still pixel-identical
    (64000/64000).
  - **client presentation**: the client used to snapshot the whole 320x200 replica for every rect
    and palette message, i.e. about 200 full-frame conversions per stream frame and 200 partial
    window updates.  It now presents once per `frame_end`: measured client CPU during continuous
    pointer motion dropped to ~5%, the link runs at the 18.2 Hz pass rate, and rows are never
    shown before the palette that belongs to them (the source of the flicker).
  - **diagnostics**: the client's stats line now ends with `fps=`, `--keyframe-every S` asks for a
    full frame every S seconds, and the engine sends a debug control message with its build
    marker (the client prints `engine debug: ...`).  With a full 320x200 keyframe every 50 ms the
    link sustains 19-20 frames/s (0.7 MB/s) and the client stays at ~5% CPU, so a lower rate on a
    machine points at the emulator side, not at the link.  The video ring grew from 8 MiB to
    64 MiB: at full-frame rates a small ring wrapped every few seconds and each wrap restarted
    the client from a keyframe, which read as a periodic palette flash.
  - **tick-driven present**: the intro drew 15-45 changes/s in DOSBox but the client received
    about 0.7 frames/s.  The pass was hooked into the input poll, the key wait and the tick delay;
    the intro animates without polling input between frames, so the seam only saw the final frame
    of each transition.  `fn_SeamPresent` is now also called from `fn_TickISR` (the game's INT 1Ch
    handler, an 18.2 Hz clock): the intro streams ~15 frames/s and the client's `shown` counter
    tracks `frames`.  The tick gate now runs before the input poll, DAC read and row scan, so the
    tight poll loops (`fn_PollInput`, `fn_WaitKey`) stay cheap; the palette is read every tick, so
    fades step at the pass rate instead of once a second.  The client gained `shown=`/`render=`
    stats, re-presents its last frame after a resize/expose, and maps the mouse through the live
    window size (integer scale plus letterbox), which fixes the pointer after a maximise.  Gate:
    oracle `SWREG.EXE: OK` + `IDENTICAL`.
  - **scene-palette ordering**: a logo switch faded out to black, then loaded the next logo's
    palette while the old logo's pixels were still on screen; the client painted the faded picture
    in the new colours ("screams"), and the VBE rows arrived over the next ten passes.  A palette
    jump larger than 24 units on any channel, with no rows in the same pass, is now held until the
    rows have streamed and the screen has been quiet for a full three-pass sweep (all 480 VBE
    rows), then sent onto the new picture; fades (small steps) still go out at once.  The hold is
    capped at 12 passes.
  - **palette ordering at the source**: frame-by-frame capture of the real client window showed
    the SW Games palette fading in while the previous publisher logo's pixels were still in the
    client's frame buffer, and the new rows arriving only after the fade.  The engine's own log
    showed the passes running, so the screen really did hold the old picture at that moment: the
    intro's fades run with interrupts masked, the tick ISR never got a pass between the steps, and
    the palette hooks sent each step on its own.  Palette hooks now run a pass themselves (same
    BIOS-tick gate as the ISR); a palette message never closes a frame on its own, so the rows and
    the palette that belongs to them travel in one frame; and after a mode set the keyframe rows
    wait out the four-pass palette hold instead of presenting with the previous palette.  Verified
    frame by frame against the DOSBox window: the publisher logos now fade in and out on their own
    pixels in the same order and with the same colours as the reference, with no wrong-colour
    frame.  Live run: the intro still streams ~15 frames/s and the client renders at ~2 ms/frame.
- 2026-10-21 — bundle packaging aligned:
  - both bundles now share one shape: `client/` (the binary plus its SDL3), `server/` (the DOSBox-X
    profile, `game/`, `capture/`), `sample/stream.bin`, a root `README.txt`, and the launchers in
    the root — `client-start.sh|bat`, `server-start.sh|bat`, `play.sh|bat`, `smoke-test.sh|bat`;
  - Windows is no longer the odd one out: `Yt2Client.exe` and `SDL3.dll` moved from the bundle root
    into `client/`, `server/README.txt` is gone (the root README covers both sides), the leftover
    `build/dist/publish-win-x64` staging directory is no longer produced, and `play.bat` now matches
    `play.sh` (server in its own window, stopped when the client exits);
  - `docs/client.md` and `docs/layout.md` updated; the two trees now differ only in the launcher
    extension, the SDL3 library name and the archive format.
  - **busy-wait screens kept their palette**: the story screen after the logos is drawn and faded
    in while the game sits in a busy wait (no input poll, no delay hook), so the only passes were
    tick-ISR passes, which must not read the DAC.  A palette hook whose pass was rate-limited now
    samples the DAC itself and leaves it for the next pass; the sample and the rows share a frame.
    Verified with a 75 s run: the client shows the gated story screen after the logos instead of
    staying black until the game starts.
- 2026-10-21 — palette at the scene boundary (the title flash):
  - the client painted the title screen in the VGA default palette for a frame or two right after
    the intro.  The game sets mode 13h (the BIOS reloads the DAC with its default palette), draws
    the title, then shows its saved "current" palette (the default) and fades it to black before
    fading in the real palette.  The engine sent the default palette (a DAC readback after the
    mode set, and the hooked block write) and the title rows before the fade-out's black reached
    the client, so one frame paired the new rows with the default colours;
  - palette hooks now stage the exact bytes of the write they are about to make (`DS:SI` for
    `fn_WriteDAC`, `ES:DX` for the block call), so a fade step or a scene palette reaches the
    client at the moment it happens.  Regular passes no longer read the DAC: every game palette
    write is hooked, and a readback after a mode set is the BIOS default, not the picture's
    palette.  The DAC is read only for a forced send (keyframe), where the client's picture is
    cleared anyway;
  - a tick pass may send the staged palette with its rows (bytes only, no DAC access), so a screen
    drawn in a busy wait gets its palette in the same frame; `seam_pendflag` and the DAC sampler
    are gone;
  - a scene-sized jump (more than 24 units on any channel) that arrives with rows holds its frame
    for one grace pass, and a held frame takes the next staged palette immediately (bypassing the
    quiet-jump hold), so the title blit with the default palette is replaced by the fade-out's
    black before it is presented;
  - verified by reconstructing every frame from `STREAM.BIN` in four 75 s runs: no frame pairs the
    title rows with a bright palette, the logos still fade in and out on their own pixels, and the
    story screen and the gated story text appear in colour; oracle `SWREG.EXE: OK` + `IDENTICAL`,
    seam pin OK, replay OK, link OK (2958 messages byte-identical).
- 2026-10-21 — palette mode-boundary and ESC-skip follow-up:
  - a mode change detected inside a palette hook now preserves that hook's staged palette (the
    initial VBE logo); a mode change detected on another pass discards the old-mode staged value;
  - once a game palette write establishes the current mode's palette, DAC reconciliation catches
    writes outside the hooks; tick-ISR samples wait two quiet ticks after a palette hook and never
    run while one is active. Mode entry invalidates the baseline so the BIOS default is not adopted;
  - the timed ESC capture showed a near-full menu redraw arriving with staged intermediate palettes
    (sums 1,556 and 10,414) before the full menu palette (23,462). Mode-13h passes that emit at least
    160 changed rows now keep the frame open through one bounded extra present pass, so the client
    presents the rows only with the final palette; stream reconstruction confirms `frame_end` follows
    the 23,462 palette. The VBE logo and post-skip menu both render in color;
  - oracle verification: `SWREG.EXE: OK` + `IDENTICAL`; seam pin OK; file variant layout check and
    `tests/replay/run.sh` pass.
- 2026-10-22 — the live link check is byte-exact again:
  - the file backend batches its writes, so the file copy lags the wire by the frame that is
    still in flight; the check read that lag as a mismatch ("17 of 4688 messages differ").
    `tools/stream/compare_streams.py` now compares by sequence number: the messages the file
    copy does not have yet are reported as in flight, a gap in the received stream is a
    failure, and a message that is in both has to match byte for byte;
  - the seam's flush was not re-entrancy safe: a tick during the DOS write could send into the
    same TX buffer, and the batched file copy and the wire copy could then differ.  A flush
    now blocks sending while it writes, and the file write retries a short write, so both
    copies carry the same bytes;
  - the serial write waited for the transmitter holding register to be empty only, and wrote
    16 bytes at a time: the emulated 16-byte FIFO could still be full, and a full FIFO
    overwrites the oldest byte, which corrupted the stream.  The write now waits for the FIFO
    to drain, and the stall budget is long enough that a link that pauses for a moment does
    not cost a message;
  - the client reads a stream that ends inside a frame as the end of the stream, not as a
    broken link, and asks for a larger socket receive buffer;
  - the emulated serial link is lossy in its own right: when the host's TCP thread stalls,
    the emulator's nullmodem buffer overflows and a chunk of the stream is lost.  That shows
    up as a gap in the received stream, so the check runs the session twice and fails only
    when both sessions lose data;
  - gates: `SWREG.EXE: OK` + `IDENTICAL` (seam pin OK), the four seam variants assemble with
    0 warnings, `tests/replay/run.sh` OK, `tests/window/run.sh` OK, `tests/link/run.sh` OK.
- 2026-10-22 — the serial transport is marked obsolete:
  - the folder link is the transport the product uses: the engine writes `STREAM.BIN` and reads
    `INPUT.BIN` in its working directory, the client follows them, and a shared or mounted
    directory carries the link across a machine boundary just as well - the UART adds nothing
    there;
  - the serial backend (`-DSEAM`), the tee build, the nullmodem profile, the client's 0xFF
    unescape and the live link check stay in the tree, are marked obsolete in the source, the
    protocol and the client docs, and are kept only for fun;
  - the file transport stays what the bundles stage and what the gates use.
- 2026-10-22 — the client can scale the picture with xBRZ:
  - `--filter xbrz` scales the engine's frame in the client with the xBRZ filter ("scale by
    rules", Zenju's pixel-art scaler) instead of letting the graphics driver enlarge it block by
    block.  The implementation is the advanced form of the algorithm: a 4x4 kernel decides for
    every corner whether it has to be blended and how strongly, and only those corners are
    blended, which keeps the small features of the art and smooths the rest.  The scale factor
    is the window scale (2 to 6);
  - the implementation is an independent one of the published algorithm, and its arithmetic
    follows the reference step for step: the port was checked against the reference
    implementation on random, structured and flat images for every scale from 2 to 6, and on the
    real game frame (576 000 pixels, no difference);
  - `tests/xbrz/run.sh` keeps that honest: the client replays the sample recording with the
    filter and compares the filtered frame with the reference's output, byte for byte.  The
    window gate now also runs the client with the filter, so the scaled presentation is checked
    on screen as well;
  - the filter has to be fast: the picture is scaled on every frame, and a naive port costs
    about 20 ms for one 320x200 frame at 3x - enough to make the window judder.  Three changes
    bring that down to about 4 ms, and the live link is limited by the emulated serial link
    again (the frame rate over the file link is the same with and without the filter):
    - the reference's own trick, the table of the colour distance for every pair of channel
      differences, is the difference between 20 ms and 1 ms: a small table around the middle
      (the pairs that matter - the neighbours of a pixel - differ in a few steps) covers the
      game art, the rare rest is computed as before;
    - only the rows that changed are scaled, with the row above and below them, because a
      blend looks two rows up and down.  The engine sends one row per rectangle, so a frame
      touches a few dozen rows out of two hundred.  The sliced scale gives the same pixels as
      the full one (checked row by row), and the client tracks the changed rows;
    - the scaled pixels are the texture's own words already (0xAARRGGBB in memory is the
      ARGB8888 layout), so they go to the window without being shuffled into bytes first -
      that conversion wrote 2.3 MB four bytes at a time and cost more than the scaling.

  - what is left in the "flags near the gate" scene is the engine's own pace, not the client:
    over the file link the client delivers the same frames per second with and without the
    filter (5 to 13 frames per second in DOSBox-X, depending on the scene), and the filter
    takes 3 to 4 ms of a frame that arrives every 80 to 200 ms.
- 2026-10-22 — the serial seam variant assembles again:
  - `sub_SeamFlush` - the file backend's batched write, with the `STREAM.BIN` pseudo-ring
    wrap - is now inside `IFDEF SEAM_FILE`, and so is its call in `sub_SeamFrameEnd`.  A
    `-DSEAM` (serial) build used to fail to assemble: `seam_written` and
    `sub_SeamWrapVideo` are declared in the file-only block, but the flush body was not
    guarded, so the serial build reported four undefined symbols.  The serial backend
    writes every message straight to COM1 and never needed the batched flush;
  - the file, tee and log builds are byte-identical to before (`SWREG.EXE` md5
    `7e86bad4…`, `411906d5…`, `6c22cfce…`), the serial build now assembles with 0 warnings
    (md5 `2590a1a4…`), and the oracle stays `SWREG.EXE: OK` + `IDENTICAL` with the pin OK.
- 2026-10-22 — VIDEO codec 1 (row runs) in the seam and the client:
  - the seam encodes every changed row as runs: `00 nn` repeats the previous pixel, `01 nn vv` a
    new one, `02 nn` plus `nn` literals, `03` ends the row.  Runs of one or two pixels stay
    literals (three bytes for two pixels is not a saving) and literals are merged and flushed in
    chunks of 255, so an encoded row can never outgrow the raw one and the payload never
    overgrows the message buffer.  Both engine modes use it (mode 13h 320-pixel rows, VBE
    640-pixel rows); the codec byte in the message header carries it per message;
  - the client decodes both codecs and only rejects codecs above 1; `tools/stream/replay.py` grew
    the same decoder so the offline gate compares two independent implementations of the format;
  - sizing: a frame of the 320x200 game was ~64 KB of raw rows and is now 1 to 4 KB in flat art
    and up to ~65 KB only for full-screen noise; the live link writes ~20 KiB/s where the raw
    rows cost ~1 MB/s over the wrap (`STREAM.BIN` used to wrap 8 MiB in 15 s);
  - the encoder's return value and destination pointer had three bugs that only showed up as a
    frozen game (a wrong length made `sub_SeamSend` copy a huge payload into the TX buffer, and
    the literal flusher restored the output pointer); they were found with a standalone COM
    round-trip harness (six 640-pixel rows: a flat run, all-distinct, a checkerboard, noise, two
    long runs, mixed) that decodes the output in Python and compares it with the source;
  - measured on the same machine, the seam's pass rate is what the client shows, not the filter
    or the link: the DOSBox-X window redraws the low-res scene at 19 to 32 distinct pictures a
    second while the client receives 9 to 10, with or without a client attached (so it is the
    seam's own pace, not the transport).  Removing the tick gate lifts the client to 18 to 50
    pictures a second and leaves the game's window at the same 19 to 20 - the gate, not the
    cost of a pass, is the limit.  The DAC readback (768 port reads a pass), the file commit,
    the encoder (raw rows instead of runs) and a bounded number of rows per pass were each
    probed and left the rate unchanged; a BIOS tick is 55 ms, so a tick gate cannot present
    faster than 18.2 a second, and its passes land back to back inside one tick where they all
    see the same screen even when two are allowed.
  - pacing is therefore its own job: two passes per tick showed the client no more pictures
    (the passes bunch), and pacing off the PIT channel 0 counter - the only finer clock the
    machine has - stopped the passes in the first attempt and needs to be debugged in isolation
    the way the encoder was.  The seam is back at the tick gate, which is safe, if coarse.
- 2026-10-22 — a `fast` build, to measure the presentation gate:
  - `-DSEAM -DSEAM_FILE -DSEAM_FAST` (`build.sh --variant fast`) opens the tick gate in
    `fn_SeamPresent`, so a pass runs on every hook call.  It is a diagnostic, not a product
    build: the gate is there because a poll or wait-key loop calls the hook in a tight loop and
    a pass scans the whole screen.  Every other build assembles the same code (`IFNDEF`);
  - same machine, same scene, folder link, one run each: the client received 8.0 pictures a
    second with the gate and 23.9 with it open (frames 106 -> 470 at 10 s and 282 -> 996 at
    32 s); DOSBox-X's own window changed at 19 to 21 pictures a second in the gated run and at
    9 to 31 in the open one, and the oracle build measured 19, 10 and 32 at the same
    checkpoints - so the gate, not the work in a pass, is what the client shows, and opening it
    does not visibly slow the game in this scene;
  - that is the trade to design around: the client needs the rate, the game must keep it.
    Pacing off the PIT (the only clock finer than the 55 ms BIOS tick) is the next step and
    `fast` is how its result gets measured.
- 2026-10-22 — whole-screen pans go out as VIDEO `scroll` (type 5), and the choppiness at the gate
  is gone at its cause:
  - **the symptom, measured**: the client was choppy exactly when the game pans the camera past the
    gate in the opening scene — the one place where the whole screen changes from picture to
    picture.  A live tap of `STREAM.BIN` during that pan: 168 changed rows and 22 KB per presented
    picture, 11 pictures a second, against 50+ a second when the screen stands still.  Opening the
    tick gate (`-DSEAM_FAST`) did not help, and this is why: the cost was the row work, not the
    gate.  The server was sending a whole screen of row messages for what is a pure shift;
  - **the fix**: a mode-13h pass now scans the rows first and sends second, and scores whole-screen
    shifts of +-8 rows against the row checksums it already keeps (a pan makes this pass's row `y`
    equal the row the client had at `y-dy`).  A step that explains more rows than no shift at all,
    and at least 100, wins: one 2-byte `scroll(dy)`, the client's checksum table shifted with it,
    and then the ordinary row pass sends the rows that slid in and everything that did not move
    with the picture (the two waving flags).  No match means no message, and the pass behaves
    exactly as before, so the heuristic can only cost bytes, never correctness;
  - **numbers**, same scene, folder link, client with xBRZ: 168 rows -> 1 row and 22 KB -> 0.4 KB
    per picture; before the change the pan's pictures arrived at 11/s, now the client renders 39-44
    pictures/s in the fast build and 16-17/s in the gated one, so what is left is the 18.2/s
    BIOS-tick gate, not the seam's work.  The client scales every picture with xBRZ in 3.3 ms, so
    the filter is not the limit either.  The remaining tick-gate limit is gone in the next entry
    (a presentation budget per tick); the pan's own rate turns out to be the game's, not the
    gate's, and that is measured there too;
  - **verification**: the detector is tested as assembly (`tests/seam-scroll/run.sh`, extracted
    verbatim, ten cases against a Python model: pans of +-1..8 with and without a row that moves on
    its own, a pan past the range, a repaint, still pictures with 1 and 40 changed rows); the new
    fixtures `tests/data/sample-pan-rows.bin` and `sample-pan-scroll.bin` are the same twelve
    pictures encoded both ways and must decode identically (the scroll form is 18,887 bytes against
    202,587); `tests/replay/run.sh` renders all five recordings with the client and the Python
    decoder; and a real run's client-rendered frames show the -1 row shift the game's content
    makes, which is what pins the sign of the message end to end.  The harness builds a plain
    binary (`uasm -bin`): with `-mz` and `org 100h` every label sits 0x100 above where the image
    loads, which made the first harness read its case table out of its own code;
  - the byte gate stays green (`SWREG.EXE: OK`, `IDENTICAL`); the seam builds are not
    byte-identical on purpose (`docs/client.md`).
- 2026-10-22 — presentation is paced by a per-tick budget, and the pan's real limit is measured:
  - **why the tick gate had to go**: a pass per 55 ms tick capped the client at 18.2 pictures a
    second.  A pan advances one row per presented picture, so the game's own pan (one row per BIOS
    tick, 18.2/s) was arriving at 16-17/s with visible jitter, and scenes the game draws faster
    than one picture per tick (the intro's fades) were held at 18.2/s as well - the tap showed
    36-53/s once that cap was lifted, with the intro's mode switches at the same times as before
    (640x480 at 0.37 s, 320x200 at 3.84 s), so the game keeps its speed;
  - **a finer clock does not exist here, and the attempt is worth recording**: the PIT channel-0
    counter looks like a 0.84 us clock, but SBFMDRV reprograms the channel to divisor 48D3h
    (~64 Hz, `sub_0A67` writes 36h to port 43h, then the divisor to port 40h; its ISR keeps the
    visible BIOS tick at 18.2 Hz, `docs/sbfmdrv.md`).  The counter therefore reloads every 15.6 ms
    and 16-bit subtraction cannot measure a 25 ms pass interval at all: the first build presented
    *nothing* in a tight poll loop (the diagnostic log showed 0 passes over 300 ticks while calls
    continued).  The per-call read is also far too expensive for the hot path: measured under
    DOSBox-X, `int 1Ah` costs 0.27 us and a PIT latch-and-read 3.0 us, and the game calls the
    present hook about 1,800 times per tick in a tight poll loop (a pass per call read the clock
    on every one of them and stretched the intro threefold, 4.4 s -> 13 s);
  - **the fix**: `fn_SeamPresent` takes at most `SEAM_PASSES_PER_TICK` = 3 passes per BIOS tick
    (55/s at most), with a forced pass (mode set, resync, keyframe) bypassing the budget.  The
    tick comes straight from the BIOS data area (0040:006Ch) - a memory read, cheap enough to do
    on every hook call - and the gate is a handful of instructions.  A tight poll loop can still
    not flood the screen; a game-driven frame goes out as it is drawn;
  - **the pan's own limit, measured**: with the budget in place the pan still presents ~17
    pictures a second, and the diagnostic `fast` build (a pass on every call, no budget) presents
    the same 196-197 scroll pictures in the same window and the same ~17/s in the busiest two
    seconds.  The pan is therefore tick-driven in the original: one row per BIOS tick is all the
    game draws.  What the seam does now is deliver every one of those pictures (the tick gate
    dropped some), and what it adds beyond the pan is the faster scenes (fades up to 53/s).  A
    smoother pan than the original would need client-side interpolation, which is not planned;
  - **verification**: `tests/seam-pace/run.sh` extracts `sub_SeamTickGate` verbatim and drives it
    against the real BIOS tick under DOSBox-X: five calls in one tick give exactly
    `[1, 1, 1, 0, 0]`, a forced pass on an exhausted budget still runs and clears its flag, and
    the first call of the next tick starts the budget over (the harness retries if a tick boundary
    falls inside a group, which would legitimately reset it); `tests/seam-pace/check.py` pins
    those rules, so a wrong clock read or a broken cap fails the gate.  The call and pass counts
    above came from the log build's existing per-call (tag 7) and per-pass (tag 12) records;
  - the byte gate stays green (`SWREG.EXE: OK`, `IDENTICAL`); the seam builds are not
    byte-identical on purpose (`docs/client.md`).
- 2026-10-22 — SBFMDRV: the uninstall refusal is tested (the last open driver behaviour):
  - `sub_1A58` walks the MCB chain from the driver's own block and stops with error `0005h`
    when another program sits above it: restoring the saved `INT 08h`/`09h` vectors would cut
    that program out of the chain, and the driver cannot know whether it hooked them, so
    refusing is the safe answer.  Until now only the clean `/U` run was tested (`NO DRIVER`);
  - `test/late.asm` (`LATE.COM`) is a 9-paragraph TSR that hooks `INT 60h` (below the driver's
    `80h..BFh` API-vector search window) and answers `0A55Ah`.  It moves `SS:SP` into its kept
    block: DOS points the stack at the top of the 64 KB block, which a small TSR gives back;
  - `test/run.sh uninstall-late` installs the driver, loads `LATE.COM`, runs
    `SBFMDRV.COM /U > U.TXT`, then `TEST.COM L`.  `U.TXT` carries `Error 0005: Other program
    exist after SBFMDRV.`, and the test proves the refusal left everything alone: `INT 08h` at
    `driver:1226h`, `INT 09h` at `driver:12B2h`, the API vector at `driver:12E6h`, and `INT 60h`
    still answering - `uninstall-late: OK`.  The plain `uninstall` (`NO DRIVER`) and `api`
    modes still pass; `docs/sbfmdrv.md` 2.6/6.4 document the scenario and section 8 no longer
    lists it;
  - with that, the driver's open items are document-only (the installer's error strings) or
    unobservable in the shipped content (jumps/`FC`/percussion have crafted coverage, and the
    digitized-SFX far calls are no-ops), so M2 is the next milestone.
- 2026-10-22 — housekeeping: the folder link leads every document, and the plan's numbering is fixed:
  - **serial path decision**: the folder transport (`STREAM.BIN`/`INPUT.BIN`) is the product link.
    The COM1/nullmodem path stays in the tree - the `serial`/`tee` builds, `client/NullModemStream.cs`,
    `client/server.sh` and `tests/link/` - but every one of them is now marked obsolete, and the
    README quick start, `docs/client.md` (pieces, quick start, section order), `client/README.md`,
    `tests/README.md` and the `CONTRIBUTING.md` gate table lead with the folder link instead.
    Keeping it is deliberate: it carries the only socket-level live check and the wire notes
    (`0xFF` doubling, `multiplier` baud scaling), at ~120 lines per side;
  - **plan numbering**: there were two sections numbered 6 and no 9.  Milestones stays 6, risks
    becomes 7, the completed M0 checklist (a stale "immediate next actions" list whose every item
    was done) is gone, open items stays 8 and the progress log is 9; the cross-references in
    `README.md`, `AGENTS.md` and `CONTRIBUTING.md` follow.  Risk 1 now records the finished
    stream/OPL decode and audio-captured run (`docs/sbfmdrv.md` 6.1/6.2) instead of listing them as
    open, section 3's SDL2 leftovers say SDL3, the `(see §7)` pointer in the sound bullet points at
    `docs/sbfmdrv.md`, and the M9 C# binding item notes the bootstrap client's hand-rolled SDL3
    P/Invoke;
  - `docs/stream.md` no longer claims the file backend cannot read input: it does, from
    `INPUT.BIN` (the folder link), and only the obsolete serial backend has no input channel;
  - the progress log had two runs: the bootstrap's M0/M1/SBFMDRV entries had ended up after the
    later ones.  They now sit in date order at the top, so the section reads oldest first again;
  - no code changed, so all gates stay as they were (byte gate `IDENTICAL`, seam/client gates
    green); the last full run is the entry above.
- 2026-10-24 — AUDIO family: the seam mirrors the music state, the client decodes the family:
  - the protocol now has concrete AUDIO payload layouts (`docs/protocol.md`): `opl_write`
    (`port, reg, val`), `opl_batch` (`count`, then `count` triples), `audio_ctrl`
    (`rate`, `fmt`, `on`).  The family existed by name only ("spec-only") before;
  - **client**: `ClientState.Apply` handles family 2.  Writes land in a per-port 256-byte OPL
    register file (`client/Opl.cs`), `audio_ctrl` sets rate/format/on, and the stats line grows
    `audio=<writes>w/<batches>b/<controls>c`.  Writes never touch the framebuffer, so the picture
    path and every render gate are unchanged;
  - **seam**: `sub_SeamAudioState` runs in the size-preserving music hook and compares `v_MusicId`
    (DS, the track the game wants) with the last id sent; when it changes it emits
    `audio_ctrl(44100, 0, on)`.  A 45 s file-backend run carries three of them
    (`audio: 0w/0b/3c`) and the picture stream is unchanged (6844 rects, 846 frames);
  - **gate**: `tests/data/sample-audio.bin` (synthetic, 4,807 bytes, built by
    `tests/replay/mkaudio.py`) interleaves an `opl_write`, an `opl_batch` and an `audio_ctrl` with
    two video frames.  `tests/replay/run.sh` renders it through the C# client and the Python
    decoder (`tools/stream/replay.py` now applies family 2 as well) and compares the PPMs:
    `replay: OK`;
  - **not yet**: the driver's OPL register writes.  The writing happens inside the resident
    SBFMDRV (its `sub_0A78` writes the port pair on the 64 Hz ISR), so capturing it means hooking
    that routine in the driver segment and buffering the writes for the present pass; the client's
    register file and `opl_batch` layout are the substrate.  Client-side FM synthesis and SDL
    audio output are the other half (M6).
- 2026-10-24 — AUDIO capture: the resident driver's OPL writes reach the stream:
  - **driver hook**: the seam scans `INT 80h..BFh` for the `"FMDRV"` signature and patches
    `sub_0A78`'s five-byte prologue (`9Ch FAh 50h 52h 51h`, `0A78h..0A7Ch`) with `EAh` + the
    seam's hook offset/segment.  The hook (`fn_SeamOplHook`) runs from the driver's timer ISR,
    copies `(AH, AL)` into the current log buffer and then executes the displaced bytes before
    jumping back to `0A7Dh`; all registers, `DS`, `ES` and the caller's flags are restored, so the
    driver sees exactly the code it expects.  The driver's integrity check only reads `[0101h]` at
    the first command 6 and never re-hashes, so the patch is invisible to it (`docs/sbfmdrv.md`
    4.5/5);
  - **log and batching**: two 4 KB buffers in the seam block (`8000h`/`9000h`, 1024 writes each)
    are swapped with interrupts off; the present pass and the `fn_SeamPresentMusic` poll hook
    drain one into `opl_batch` messages of 1..192 writes (port from the driver's `0114h`,
    `0x228`) at the poll rate, since the log changes on the driver's 96 Hz tick.  The messages go
    out with `seam_inpresent` held and the batch is flushed even when the pass closes no frame, so
    a static picture does not hold the sound.  The music hook saves `DS`, `AX` and the flags (the
    game calls it with its own `DS`), and the 4 KB size covers ~12 s of writes with no drain;
  - **music state**: `sub_SeamAudioState` now reads the driver's `[02F2h]` sequencer byte instead
    of the game's `v_MusicId`; the poll sites do not all run with `DS` pointing at the game data
    (some point at buffers), which made the old read flap and flooded `audio_ctrl` (39,591
    messages in 15 s).  The driver byte changes only on play/stop (4 messages in 45 s);
  - **segments**: `sp_finish` can reach the end of a present pass with `DS` still on the seam
    segment, and a batch flushed with the game's `DS` wrote game code/`EXE` image into
    `STREAM.BIN`.  `sub_SeamSend` and `sub_SeamFlush` now load the block segment themselves (and
    the pass re-establishes it at `sp_off`), so no caller context can corrupt a batch;
  - **run**: 45 s file backend, 8,828 records / 1,200,758 bytes, 853 frames, 6,823 rects, 165
    scrolls, 625 palette messages, 353 `opl_batch` / 2,361 register writes / 4 `audio_ctrl`; the
    C# client replays it with `unknown=0` and reports `audio=2361w/353b/4c`, final frame the
    opening gate scene; melodic key-ons/offs balance 275/268 (the last chord is still held), and
    all writes use the FM base `0x228`;
  - **gates**: `verify.sh` stays `IDENTICAL`; `tests/replay/run.sh` (including the AUDIO fixture)
    stays green; the batch-write count now feeds the client's `audio=w/b/c` stat;
  - **not yet**: client-side FM synthesis and SDL3 audio output (M6), plus timestamps for the
    batches if the present-pass quantisation turns out audible.
- 2026-10-24 - AUDIO output: the client synthesises the driver's OPL stream:
  - **core**: `client/Opl2.cs` is an independent YM3812 implementation: nine
    two-operator channels, FM or additive connection, feedback, the four OPL2
    waveforms, the ADSR envelope with the YMF278B 64-rate tables, key scaling
    (KSR/KSL), total level, tremolo and vibrato.  The first attempt used a
    hand-fitted rate curve and a half-cycle modulation index; against the
    paired DOSBox-X WAV the notes died about 50x too fast and the timbre had
    almost no sidebands.  The rate tables and the chip's four-cycle (8*pi)
    modulator index bring it in line;
  - **output**: `client/Opl.cs` owns one chip and one 256-byte register file
    per FM port; `client/SdlAudio.cs` opens an SDL3 s16 stereo stream at
    44100 Hz and keeps ~100 ms queued.  Rendering runs on the main thread, the
    reader thread only applies writes.  `--headless` and `--no-audio` leave the
    device closed.  The shipped DOSBox-X profiles now set `nosound = true`:
    the emulated SB/OPL still runs and the seam still sees its writes, but
    the host plays only the client, not a second delayed copy;
  - **reference check**: `tests/data/music-walk.dro` is a 2.3 KB DRO capture of
    the real SBFMDRV playing one WORLD.DAT block; `--dro/--wav` renders it
    through the same core.  Against the paired 14.69 s DOSBox-X WAV, the
    12.66 s render has peak 41.2% FS / rms 2261 vs 41.8% / 2310, the envelope
    follows the reference, and the first bars' pitches match;
  - **gates**: `verify.sh` `IDENTICAL`; `tests/audio/run.sh` (new) checks the
    DRO render's duration, level and pitches and requires the dummy SDL device
    to push frames; `tests/replay/run.sh` stays green; the 45 s client replay
    reports `audio=2361w/353b/4c`, `unknown=0`; a live 20 s folder-link run
    decoded 1,694 writes / 241 batches / 4 controls with the dummy device
    open;
  - **not yet**: per-write timestamps; v0 applies each batch at the present
    pass that carries it, so rhythm can be quantised by tens of milliseconds.
    The live link has no audible check on this machine (no sound device); the
    dummy device and the DRO reference are the gates here.
- 2026-10-24 - AUDIO timestamps and the SFX question:
  - **engine**: `fn_SeamOplHook` now appends `(u32 PIT, register, value)`
    (6 bytes per write, 682 per buffer).  The stamp is the BIOS tick at
    `40:006Ch` times the driver's reload `CS:0141h` plus its tick phase
    `CS:0147h`, read under `cli`; the driver zeroes the phase at every music
    start, so `seam_opl_adj` adds the lost part and the stamps never step
    back.  `sub_SeamAudioFlush` sends `opl_batch_timed` (type 3: port, count,
    then 6-byte records, 1..128 writes per message); the untimed type 1 stays
    in the protocol for older recordings;
  - **client**: `client/Opl.cs` schedules timed writes on the render timeline
    (the first anchors at the render front, later stamps advance by their
    modular difference, a late write plays at the front) and splits its
    render blocks at event frames; `ClientState` parses type 3.  `--file
    rec.bin --wav out.wav` renders a recording's audio offline (`StreamAudio`
    + `Opl.FreeRun`), so a folder-link capture can be checked or heard without
    the engine.  `tools/stream/replay.py` decodes type 3 too;
  - **verification**: a 20 s file run carries 240 timed batches / 1,699
    writes / 4 controls over 353 frames / 2,073 rects; no batch is
    non-monotonic, the stamps advance by 12,428 counts (96 Hz) inside a music
    sequence, and the span matches the wall clock.  `tests/audio/run.sh`
    encodes `tests/data/music-walk.dro` as timed batches, replays it with
    `--file --wav` and gets a WAV byte-identical to the direct DRO render.
    A live 20 s folder run with the dummy device decoded 1,774 writes / 254
    batches / 4 controls, `unknown=0`, and an instrumented 18 s run scheduled
    1,657 writes with none behind the render front (`late=0`);
  - **SFX**: `fn_PlaySFX` needs `v_SoundState` bit 3, which only
    `sub_SoundPreInit` sets; the chapter skips that path (the capture shows no
    FM-detect writes from it), and the options toggle is gated on the
    pre-init bit, so this configuration has no OPL sound effects.  Only
    effect 3's PC-speaker beep (`fn_SpeakerBeep`, PIT channel 2) remains, and
    the protocol does not carry it; M6's wording now says so.
- 2026-10-24 - PC-speaker beep:
  - **engine**: the OPL hook's stamp logic moved into `sub_SeamStamp` (same
    clock, same `seam_opl_adj` shift) so the speaker hooks share it.
    `fn_SpeakerBeep`'s PIT and gate writes are replaced by size-preserving
    far calls (plus nop padding) to `fn_SeamSpeakerOn`/`fn_SeamSpeakerOff`;
    the stubs execute the displaced instructions and send
    `speaker(gate,divisor,pit)` (AUDIO type 4: gate, u16 divisor, u32 PIT)
    on the same timeline as the register writes.  Keeping `S06E7` the same
    size matters: a 10-byte growth shifted every later segment and froze the
    intro after the logo, which is why the first version (and its S1DC9 pad)
    was replaced.  The oracle build excludes the blocks and stays
    byte-identical;
  - **client**: `ClientState` parses type 4, `Opl.Speaker` schedules the edge
    at its stamp and `Render` mixes a 30% FS square wave at
    `1,193,182/divisor` between on and off (reset phase at each edge); the
    `speaker=` count joins the stats line and `--file --wav` renders it
    offline;
  - **verification**: `verify.sh` 0 warnings, `SWREG.EXE: OK`, `IDENTICAL`;
    `tests/data/sample-audio.bin` gained the on/off pair (divisor 662,
    220 ms) and `tests/audio/run.sh` checks the 1.47 s render, 30% rms,
    ~1800 Hz and that the tone stays inside the gate window.  The first,
    size-growing hook emitted the pair in a 20 s run (divisor 662,
    262,140 counts = 219.7 ms apart) but shifted `S06E7` and froze the intro
    after the logo; with the size-preserving stubs a temporary one-shot call
    at `fn_SeamPresentMusic` beeped through the real `fn_SpeakerBeep` in a
    40 s run (one on/off pair, divisor 662, 262,140 counts apart,
    non-decreasing stamps) while the game ran normally: 763 frames / 2,296
    writes / 341 batches / 4 controls.  The temporary call was reverted
    before the commit; `tests/replay/run.sh` stays green;
  - **not yet**: the audible check still needs a machine with a PCM device.
- 2026-10-25 - WORLD.DAT VOC sound effects:
  - **data**: the game's 111 `fn_PlaySFX` call sites use 80 Creative Voice
    File records in WORLD.DAT (8-bit unsigned PCM, 4000–14925 Hz, 0.1–5 s).
    The SFX pointer table is at `DS:0CF51h` (80 entries, `v_5160`); `fn_183BD`
    reads the record, but
    the far entry it calls (`v_SoundEntry`) is zero and SBFMDRV 1.34 has no
    digitized path, so the reference configuration only beeps for effect 3.
  - **engine**: `fn_PlaySFX`'s first six-byte `TEST` is replaced, in the seam
    build only, by a far call plus nop to `fn_SeamPlaySFX`; the stub preserves
    every register/flag, sends `sample(id, offset, pit)` (AUDIO type 5: u16
    id, u32 offset from the game's own table, u32 PIT stamp) on the shared
    stamp timeline and then executes the displaced test.  The oracle branch
    keeps the original instruction and the reference image stays identical;
    the pin was refreshed for the guarded hook.
  - **client**: new `client/Samples.cs` parses the VOC header/blocks (codec 0,
    type 1/2/8/9) and caches by offset; `Opl` schedules a sample start on the
    PIT timeline, mixes up to eight records with linear resampling at 37.5%
    FS, and drops effect 3's following speaker pair when the sample is
    available.  `--world PATH` overrides the default WORLD.DAT lookup (next to
    `--follow`/`--file`, then `server/game/WORLD.DAT` next to the client).
    `--file --wav` renders samples too, extending the tail to the last record;
  - **verification**: `verify.sh` 0 warnings, `SWREG.EXE: OK`, `IDENTICAL`;
    `tests/audio/run.sh` renders the real effect 3 (`09FFE6h`, 0.20 s) with
    its beep pair and alone (identical, so the beep was dropped), effect 5
    (`0A076Ah`, 1 s at 345 Hz) alone (2.00 s, rms 1488; 345 Hz dominates
    1800 Hz) and a beep-only control; `tests/replay`, `tests/xbrz`,
    `tests/window` stay green.  A 95 s keyed run (`STREAM_KEYS="62:Return
    63:space ..."`) carried 9 sample messages, ids 16–25 with offsets
    matching `0CF51h`; the client rendered 95.51 s with 9/9 played /
    0 unknown, sample windows rms 2.2k–4.7k against ~1.1k between lines;
  - **not yet**: the audible check still needs the user's PCM device; the
    offline WAV and the live stream counts are the gates on this host.
- 2026-10-25 - M1c: timed, bidirectional asm-server session capture/replay:
  - **format**: the optional `YT2S` wrapper records complete v0 frames from
    both directions. One monotonic client clock stamps an engine frame when
    fully received and a client frame after its write succeeds. Its timestamps
    describe boundary-observed time, not the exact game instruction that
    consumed an input;
  - **client**: TCP runs capture a full-duplex trace by default under
    `build/runs/session/<stamp>/session.yt2s`; `--record-session` chooses another
    path and `--no-record-session` disables capture. `--file` detects `YT2S` and
    replays engine output at its captured pace (`--session` remains an alias).
    `--replay-input` accepts timed INPUT records as well as the YT2S capture and
    schedules game INPUT frames into a live `--follow` server (with a fresh
    handshake);
  - **asm-server run**: a 12 s DOSBox-X run of the file build produced a
    190,623-byte session with 169,469 bytes / 1,311 engine messages, the
    handshake and a scripted Return at 7 s. Replaying the session displayed
    159 frames over 12.84 s; feeding its input into a fresh asm-server run
    delivered the recorded key frame;
  - **gate**: `tests/session/run.sh` uses a fake full-duplex peer to check both
    directions, nondecreasing timestamps, a 0.26 s output gap, paced playback
    and timed INPUT replay. `dotnet build client -c Release` and the existing
    stream decoder gates remain green. With M1c closed, M2 is next.
- 2026-10-26 - timed folder-link playback (`YT2B`):
  - **transport**: `STREAM.BIN` and `INPUT.BIN` now wrap each unchanged v0
    frame with a PIT timestamp and length. The ASM server timestamps output on
    its monotonic PIT timeline and unwraps client input; the client stamps
    input on its own monotonic PIT clock (seeded from the first server frame
    only before any input is written), auto-detects timed `.BIN` playback, and
    retains raw fixture plus YT2S compatibility;
  - **sample/bundles**: added a deterministic timed engine-output fixture from
    `sample-video.bin`; Linux and Windows publishers stage it as
    `sample/STREAM.BIN`, and smoke-test launchers play that file. Live folder
    launchers no longer require a separate `.yt2s` capture;
  - **first-frame clock fix**: replaying a captured `INPUT.BIN` exposed that
    the client re-anchored its input clock when the first server frame arrived
    after the handshake; a 0.8 s key could then be written with a 4-5 s gap.
    The input clock now keeps one client epoch, seeded from the server only
    when no input has been written yet, and the session gate checks that the
    gap stays sub-second;
  - **verification**: `verify.sh` reports `SWREG.EXE: OK` / `IDENTICAL`;
    `tests/session/run.sh` checks YT2S compatibility, timed BIN paced playback,
    both Python stream tools, timed input replay and the late-anchor clock;
    `tests/replay/run.sh` compares the C# and Python renders for the timed
    sample byte-for-byte. A 15 s DOSBox-X run produced 1,849 records in
    282,337 bytes spanning 14.897 s; a separate live run consumed a timed
    `INPUT.BIN` keyframe request (set_mode 1 -> 3) and produced 934 records in
    8.923 s.
