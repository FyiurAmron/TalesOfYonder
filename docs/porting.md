# YT2 — Yendorian Tales Book I, Chapter 2: port plan (v1, draft)

Recreation of `SWREG.EXE` (201,810 bytes, MD5 `c3bdd3054e55a1e0b57c9780654dc982`) for modern
platforms. This document is the agreed working plan; it supersedes ad-hoc notes.

Milestones, open items and the next steps are in `docs/roadmap.md`; the dated work
log is in `docs/progress.md`.  This document is the design and verification plan.

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

Target: C11, gcc/clang/msvc; the engine core has no host dependencies. SDL3
belongs to the companion/direct host, not the core. Runs on >= 32-bit systems.

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
- video: 8-bit framebuffer + palette behind a sink; the companion client presents it (the
  folder `YT2B` records are the first backend). An optional direct SDL3 host can share the
  same interface later;
- input: scripted events (harness) plus companion-client input mapped onto the existing dispatch
  tables / int 33h equivalents, incl. invoking the game's user handler; SDL3 capture belongs
  to the host/client;
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
  The asm seam ships on a shared folder (`OUTPUT.BIN`/`INPUT.BIN`, `docs/client.md`); the earlier
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

## 6. Risks / unknowns (ordered)

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

## 7. Roadmap and progress

The milestone table, the open items and the next steps live in
`docs/roadmap.md`.  The dated work log, including the evidence behind each
milestone, lives in `docs/progress.md`.
