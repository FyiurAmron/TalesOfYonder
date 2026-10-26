# YT2 companion protocol — v0 (draft)

The engine core and the client/host speak one message protocol. The same messages are used for
in-process clients, IPC clients and **recordings**; the transport is a detail. Tests target this
boundary.

There is one transport left: the engine writes `STREAM.BIN` and reads `INPUT.BIN` in its working
directory, and the client follows those two files. The directory can be local or shared, so the
link crosses a machine boundary without a network protocol of its own. The earlier serial
transport (protocol-v0 over COM1 and a nullmodem) is **obsolete** and kept only for fun.

## Roles and ownership

| Side | Owns |
|---|---|
| engine | game state, memory + EMS, logical clock, determinism, file *names/handles/offsets* |
| client | display state (framebuffer replica, palette, mode), window/scaling/filters, OPL synthesis, input capture, debug overlays |

- The engine never owns host resources; the client never owns *game* state.
- In-process operation is the degenerate transport: direct calls, same message structs.
- The client role survives into the C# product (engine = core library with a formal host
  interface; app = host/client). The split is architecture, not scaffolding.

## Framing

Little-endian, transport-independent, no alignment assumptions.  The magic is `0x5954`, so the
first two bytes on the wire are `54 59` (`'T' 'Y'`) — the reverse of how it reads.

```
u16  magic 0x5954 ('YT')
u8   version
u8   family          family id (below)
u8   flags           bit0 compressed, bit1 ack_requested, bit2 eof, bits3-7 reserved
u8   codec           payload codec id (0=raw, 1=row runs, later 2=LZSS)
u32  seq             per-direction sequence number (replay integrity)
u32  len             payload length before decoding
<len bytes payload>
```

- Codecs are stateless and side-effect-free; a receiver handles any codec it advertised.
- Unknown family/flags: ignore + `error` (forward compatibility).
- v0 live frames have no general timestamp; the sequence number orders one
  direction only. AUDIO's PIT stamps are local to its audio timeline and do not
  time VIDEO or INPUT.
- A transport that can hand over a stream mid-message (a live link joins a running engine, for
  example) may resynchronise on the magic; version/family/length sanity checks make a false
  boundary practically impossible.

## Families (v0)

| id | name | direction | messages |
|---|---|---|---|
| 1 | VIDEO | engine to client | `set_mode(w,h,palette?)`, `palette(start,data)`, `rect(x,y,w,h,payload)`, `frame_end`, `reset`, `scroll(dy)` |
| 2 | AUDIO | engine to client | `opl_write(port,reg,val)`, `opl_batch(payload)`, `opl_batch_timed(payload)`, `audio_ctrl(rate,fmt,on)`, `speaker(gate,divisor)`, `sample(id,offset)` |
| 3 | INPUT | client to engine | `key(scancode,down)`, `mouse_move(dx,dy)`, `mouse_button(btn,down)`, `quit` |
| 4 | TICK | both | `tick(n)`, `pace(ms)` (client may pace; engine never reads wall clocks) |
| 5 | CONTROL | both | `hello(role,version)`, `bye`, `error(code,text)`, `debug(channel,payload)` (product clients ignore `debug`) |
| 6 | FILES | - | retired: the folder transport already shares the game directory, so a host file service adds nothing |

### Encoding policy ("smart but trivial")

Per-family, not one-size-fits-all:

- VIDEO: only dirty rectangles; inside a rect, rows are encoded with skip/literal/repeat opcodes
  (unchanged runs become skips). A picture that moved as a whole (the game's camera pans) is one
  `scroll` message plus the rows that slide in, never one rect per row. Palette changes are separate
  messages, so palette animation costs a few bytes. No full-frame rebuilds unless a mode/reset
  forces them.
- AUDIO: OPL *register writes* and PC-speaker gate edges travel, not PCM; synthesis is
  client-side. Messages are tiny, ergo no compression.
- INPUT/TICK/CONTROL: never compressed (overhead exceeds payload).
- Codec id is per-message; adding a codec must never change semantics, only size.
- Rule of thumb: any codec must be implementable in ~100 lines in asm, C and C# - that is the bar
  for "shared and trivial".

### Codec 1: row runs (VIDEO rects)

Codec 0 is the raw form: a rect payload of `w*h` pixels, row-major, one byte per pixel. Codec 1
replaces those pixels with runs, per row, still after the 9-byte rect header
(`kind, x, y, w, h`):

| opcode | bytes | meaning |
|---|---|---|
| `00 nn` | 2 | repeat the previous pixel `nn` times (`nn` = 1..255) |
| `01 nn vv` | 3 | pixel `vv` repeated `nn` times |
| `02 nn <nn bytes>` | 2+nn | `nn` literal pixels |
| `03` | 1 | end of row |

Rules:

- Every row is encoded independently and starts with an `01` or `02` (nothing precedes it, so `00`
  is invalid there). Each row ends with `03`; a row is short/long if the counts do not add up to
  `w`, which is a protocol error.
- Runs of three or more pixels use `00`/`01`; one- and two-pixel runs become literals, because
  three bytes for two pixels is not a saving. Literals are merged into chunks of at most 255, so
  the worst case is the raw size plus one opcode and one count byte per 255 pixels - a rect can
  never outgrow the raw form, and a flat row is 4 bytes instead of 320.
- The opcode byte only describes the row, so the rect header (and therefore the client's
  dirty-row bookkeeping) is unchanged: decoding writes exactly `w` pixels per row.
- Message framing is untouched: `codec` in the 14-byte header is 1, and the payload length covers
  the whole encoded rect. Non-rect messages are always codec 0.
- The engine only emits it once a row changed (the shadow checksum says so), so the codec is a
  pure size/speed trade for the row itself, never a correctness one.

Decoding is a straight line: keep the previous pixel and a cursor, and stop at `03` or at `w`.
Encoder, C# decoder and the Python reference decoder are covered together by
`tests/replay/run.sh` over `tests/data/sample-video-rle.bin`.

### Whole-screen pans (VIDEO `scroll`)

A panning camera redraws every row of the screen, which as rects costs a row message per row -
on the game's gate slide-down that was 168 messages and 22 KB per presented picture, and the
engine could only afford 11 pictures a second. The same picture is one small message plus the
rows that slide in:

| type | message | payload after `u8 type` |
|---|---|---|
| 5 | `scroll` | `i8 dy` (positive: the picture moved down by `dy` rows) |

Rules:

- A `scroll` moves the client's replica by `dy` rows: for `dy > 0` row `y` takes the old row
  `y-dy`; for `dy < 0` row `y` takes the old row `y+|dy|`. The rows that slide in did not exist
  before and are always sent as ordinary rects **in the same frame, after the scroll**; every row
  that did not move with the picture (a sprite, an animated flag over a panning background) is
  sent the same way. Apply the scroll to what has arrived so far, presented or not.
- Order matters inside a frame: `scroll` first, then the rects. A `scroll` with no rect after it
  is legal and means the whole picture was the old picture moved.
- `|dy|` must be smaller than the client's height (a jump bigger than that is a repaint, not a
  pan); `dy = 0` is a no-op a client may ignore.
- The `codec` field is 0: a scroll has no pixel payload.
- Detection is engine-side and cheap: the seam already keeps a 32-bit checksum per row of what the
  client has, and a pan makes this pass's row `y` equal the checksum of the row at `y-dy`. The seam
  scores every step in +-8 rows and takes the best one **only when it explains more rows than no
  shift at all and at least 100 rows**; otherwise the rows go out as rects exactly as before. So
  the game stays correct when the heuristic does not fire, and a step costs 2 bytes when it does.
  The client's checksum table is shifted with the message, which is what makes the following rows
  comparable again.
- A pan beyond +-8 rows per presented picture falls back to rects: correct, just not cheap. The
  detector is covered by `tests/seam-scroll/run.sh` (the asm routine against a Python model of
  these rules), and `tests/data/sample-pan-rows.bin` / `sample-pan-scroll.bin` are the same twelve
  pictures encoded both ways, which `tests/replay/run.sh` renders with both decoders.

### Client display state

- The client holds display state: `mode` (w x h), an 8-bit indexed **framebuffer replica**, and the
  256x3 palette. VIDEO messages update it; `frame_end` presents (scaling, filters, overlays).
- The engine stays authoritative: game code reads back video RAM (sprite masking, overlays,
  probes), so the engine's memory is the source of truth. VIDEO is strictly one-way
  (engine -> client); the replica is never read back.
- Deltas are computed engine-side: the engine compares its linear framebuffer against a shadow of
  the last presented frame and emits changed rects, and each send pass ends with `frame_end`
  (implementation detail). A pass whose only change is the palette still ends with `frame_end`, so
  palette animation reaches the client without a repaint.
  For the mode-13h game screen the delta engine compares against a shadow of the last presented
  frame.  The VBE screens (the game uses 640x480x8 for the publisher logo and 640x400x8 for the
  title and later screens) have no second 300 KB buffer: each row is read through the VBE window
  (function 4F05h, window A) and compared with a 32-bit checksum kept per row, so a row is only
  resent when its checksum changes (a collision costs one stale row, not a desync).  VBE banking
  stays engine-internal: those rows are still `rect(0,y,640,1)`, and the client never sees a bank.
  `set_mode` carries the height, and a change of VBE mode (or of its height) clears the checksum
  table and sends a fresh `set_mode`, exactly like entering VBE from 13h.
- A `scroll` moves the replica and marks the whole frame dirty; the rows that slid in arrive as
  rects behind it. A `set_mode` means "start from a blank screen": the client drops its replica,
  reallocates at the new size and expands the palette again.  The engine relies on this — it does not send a full
  repaint just because the mode changed.
- A pass carries at most ~16 KB of VBE rows, so a full intro repaint is spread over a few passes
  instead of stalling the game for a frame time.
- A full-frame rect can serve as a display-state checkpoint. `YT2B` and `YT2S`
  do not insert periodic keyframes or build a seek index, so playback starts at
  the beginning. On a live link a **keyframe is requestable**: the folder
  client sends CONTROL `keyframe_request`; the obsolete serial link also
  accepts one bare byte `'K'` (0x4B). The engine answers with `set_mode`, the
  full palette and a full-frame pass, letting a client connect to an
  already-running game.
- Palette traffic is coalesced engine-side: hooks record writes, and a send carries the span from
  the first to the last entry that differs from the palette the client already has. The span may
  contain unchanged entries; clients must not assume it is minimal.

### Payload layouts (v0)

The first payload byte is the message type (per family). Multi-byte fields are little-endian and
unpadded. VIDEO, CONTROL and INPUT (keys) are implemented in the seam builds (see
`docs/stream.md`); AUDIO is implemented in the seam builds (they emit the
driver's register writes and the PC-speaker gate) and consumed by the client
(it keeps the per-port register file, synthesises the samples and renders the
beep, `docs/client.md`); TICK, mouse INPUT and FILES are still spec-only here:

| type | message | payload after `u8 type` |
|---|---|---|
| 0 | `set_mode` | `u16 w`, `u16 h`, `u8 bpp` |
| 1 | `palette` | `u16 start`, `u16 count`, `count*3` bytes of 6-bit DAC R,G,B |
| 2 | `rect` | `u16 x`, `u16 y`, `u16 w`, `u16 h`, then `w*h` 8-bit indices row-major (codec 0) or row runs (codec 1, below) |
| 3 | `frame_end` | *(empty)* |
| 4 | `reset` | *(empty)* |
| 5 | `scroll` | `i8 dy` (see "Whole-screen pans") |

AUDIO (family 2), emitted by the seam builds and consumed by the client:

| type | message | payload after `u8 type` |
|---|---|---|
| 0 | `opl_write` | `u16 port` (LE), `u8 reg`, `u8 val` (length 5) |
| 1 | `opl_batch` | `u8 count`, then `count` × (`u16 port`, `u8 reg`, `u8 val`) (length `2 + 4*count`) |
| 2 | `audio_ctrl` | `u16 rate` (LE), `u16 fmt` (LE), `u8 on` (length 6) |
| 3 | `opl_batch_timed` | `u16 port` (LE), `u8 count`, then `count` × (`u32 pit`, `u8 reg`, `u8 val`) (length `4 + 6*count`) |
| 4 | `speaker` | `u8 gate`, `u16 divisor` (LE), `u32 pit` (LE) (length 8) |
| 5 | `sample` | `u16 id` (LE), `u32 offset` (LE), `u32 pit` (LE) (length 11) |

- `opl_write` streams one OPL register write; `opl_batch` is the untimed
  coalesced form kept for older recordings; `opl_batch_timed` is what the
  current seam builds send.  The driver hook batches 1..128 writes, all with
  the FM register base (`0x228` with the usual `BLASTER=A220`), and a client
  applies a batch in order.
- `u32 pit` is the write's time in 8253 counts (1,193,182 Hz): the BIOS tick
  at `40:006Ch` times the driver's tick reload (`CS:0141h`), plus the driver's
  tick phase (`CS:0147h`).  Writes from one driver tick carry the same stamp,
  so a batch keeps the ticks' ~10.4 ms spacing even when several ticks land in
  one present pass.  The origin is arbitrary and only differences matter.
  The driver zeroes its phase at every music start; the seam adds the lost
  part to the timeline, so stamps are non-decreasing (a client may still see
  a wrap: the count is derived from the low BIOS-tick word and rolls over
  about hourly, so it must compare with a modular difference).  The reset
  happens in game context between ticks, so the exact instant is only known
  to the tick: the first gap after a music start can be up to one tick short
  (measured: 0.1 and 6.6 ms on a 20 s run).
- A timed write takes effect at the frame the client's render timeline
  reaches its stamp; writes that map behind the render front are played at
  the front (late), which is the old untimed behaviour.  The client keeps
  ~100 ms queued, so a batch that arrives a frame late still schedules its
  ticks in the right order.
- `audio_ctrl(rate,fmt,on)` carries the DAC rate/format the client should
  synthesise at and whether the music sequencer is on.
- `speaker(gate,divisor,pit)` carries the PC-speaker gate that
  `fn_SpeakerBeep` drives: `gate` 1 starts a square wave at
  `1,193,182 / divisor` Hz (`divisor` 662 in the game, ~1.8 kHz), `gate` 0
  stops it, and the stamp uses the same timeline as the OPL writes.  In the
  SBFMDRV configuration this is effect 3's fallback; the `sample` message
  below replaces it when the client has WORLD.DAT (see `docs/client.md`).
- `sample(id,offset,pit)` carries one `fn_PlaySFX` call.  `id` is the game's
  effect number and `offset` is the WORLD.DAT file offset from the game's SFX
  pointer table (`DS:0CF51h`); the record is a Creative Voice File (8-bit
  unsigned PCM in every shipped record).  Only the id, offset and stamp
  travel: the samples are up to 35 KiB, and the client reads the file next to
  `STREAM.BIN`.  The stamp uses the same timeline as the OPL writes.  `id` 3
  is the effect whose immediately following PC-speaker fallback pair the
  client drops when it has the sample.

- Palette values are DAC values (0..63), not 8-bit RGB; clients expand them for display.
- Compared with the M1b spike (one full 256-entry palette per fade step), the
  palette message is now a diff span and `frame_end` closes every pass. The v0
  frames remain unchanged; the folder transport wraps each complete frame in a
  timed `YT2B` record (`STREAM.BIN` engine output, `INPUT.BIN` client input).
  VIDEO uses `h=1` rects (one message per changed row, at most 640 wide), in
  320x200x8 for the game and 640x480x8 for the VBE intro. Raw v0 files remain
  supported for protocol fixtures.
  Rows travel as raw pixels (codec 0) or as runs (codec 1); the seam build uses codec 1 and a
  recording can mix both.  `tests/data/sample-video.bin` covers mode 13h raw,
  `tests/data/sample-vbe.bin` the intro raw, `tests/data/sample-video-rle.bin` both modes with the
  run codec, and `tests/data/sample-pan-rows.bin` / `sample-pan-scroll.bin` the same twelve-picture
  pan encoded as rows and as scroll messages.
- Types are only unique within a family; later families define their own table.

CONTROL (family 5), implemented in the seam build:

| type | message | payload after `u8 type` |
|---|---|---|
| 0 | `hello` | `u8 role` (0 engine, 1 client), `u8 version` |
| 1 | `bye` | *(empty)* |
| 2 | `error` | `u16 code`, ASCII text (the rest of the payload) |
| 3 | `debug` | `u8 channel`, opaque bytes |
| 4 | `keyframe_request` | *(empty)* |

- The engine sends `hello(0, 0)` before its first `set_mode`, so a recording says what produced it;
  it answers the client's `hello` once per session with the same message.
- `keyframe_request` is what the serial link used to spell with a bare `'K'` byte (still honoured):
  `set_mode`, the full 256-entry palette and a full-frame pass follow, which is how a client that
  arrives late synchronises.
- A client repeats its `hello` and its `keyframe_request` until each is answered (the engine's
  `hello`; a `set_mode` for the keyframe), because the first request of a session is regularly
  lost on the serial link (the engine's UART FIFO is 16 bytes and the two frames are 32).  The
  keyframe frame is 15 bytes and goes first, the 17-byte `hello` second.
- A `rect` that is well framed but does not fit the client's current mode is **not** a framing
  error: it means the engine is already drawing another mode (the client joined between the mode
  change and its `set_mode`).  The client counts it, discards it, and asks for a keyframe instead
  of resynchronising the stream; the reply's `set_mode` repairs the mode.
- A client should send `hello` right after connecting and repeat it (with the keyframe request)
  until the engine answers: DOSBox-X drops host bytes while the game has not reached the seam yet,
  so the first hello can be lost.  A second hello is ignored engine-side (it answers once).
- `error` from the engine reports a malformed or unknown message it received; the client prints it.
  `bye` is accepted and ignored: the engine cannot stop the game from the link.

INPUT (family 3), keys and mouse implemented in the seam build:

| type | message | payload after `u8 type` |
|---|---|---|
| 0 | `key` | `u8 down` (1 press), `u8 scancode`, `u8 ascii` (0 = extended key) |
| 1 | `mouse_move` | `s16 dx`, `s16 dy` (relative) |
| 2 | `mouse_button` | `u8 button` (0 left, 1 middle, 2 right), `u8 down` |
| 3 | `quit` | *(empty)* |

- The engine writes key presses into the BIOS keyboard ring at `0040:001A`, which is exactly what
  the game's DOS `06h`/`0Ch` calls drain; it ignores releases, because the ring is a type-ahead
  queue, not a key-state table.
- Scancodes are the IBM PC set-1 values; extended keys (arrows, function keys, modifiers) carry
  ASCII 0, which is what DOS returns for them.
- Mouse motion is relative and in *game pixels* (320x200 or 640x480 units), so the client has to
  divide host pixels by its integer scale; the engine converts to mickeys with the ratio the game
  asked for (`0Fh`, 8 mickeys per pixel here) and keeps the sub-pixel remainder.
- Mouse buttons are edges, not state: `down` says whether the button went down or up.  The engine
  answers `0Bh`/`03h` from what it has applied and calls the game's own `int 33h 0Ch` event
  handler, so the game's software cursor follows the client; the middle button is dropped (the
  game has no use for it), and the shim keeps the position inside the game screen (0..319,
  0..199), which is what a click latches.
- The engine only reports what INT 33h would: an app that never installs a `0Ch` handler is
  unaffected, and functions the shim does not implement are chained to the driver that was
  installed before it (DOSBox-X's own).

## Recordings / replay

- The v0 frame remains unchanged. The default folder-link files are timed
  `YT2B` streams, so `STREAM.BIN` itself replays at its recorded pace; no
  sidecar or `YT2S` conversion is needed. The older `YT2S` session format is
  still accepted; folder links can request one explicitly with
  `--record-session` (TCP clients still capture it by default).
- `YT2B` header (16 bytes): ASCII `YT2B`, `u16 format_version=1`,
  `u16 header_bytes=16`, `u32 timebase=1,193,182` PIT counts per second,
  `u8 direction` (`0` engine-to-client, `1` client-to-engine), three zero
  reserved bytes. Each event is `u32 pit_stamp`, `u32 frame_bytes`, then one
  complete v0 frame including its 14-byte header. Records are appended in
  order; the 32-bit PIT stamp wraps modulo 2^32 (about hourly), so readers use
  modular differences. Playback anchors the first event at time zero.
- `STREAM.BIN` stamps are taken by the asm server on its monotonic PIT timeline
  (the same timeline used by AUDIO; without the resident timer driver it falls
  back to BIOS-tick resolution). `INPUT.BIN` stamps use the client's monotonic
  PIT clock. When the first server frame arrives before any client input has
  been written, its stamp seeds the client epoch; once input exists the epoch
  is fixed, so the handshake written before that frame cannot make later input
  jump. The timestamps are retained for timed input replay; the asm server
  unwraps each frame and applies it when it polls the file, without changing
  game timing.
- The v0 frame remains a pure protocol message: its sequence number orders one
  direction, while the enclosing YT2B record supplies transport time. This is
  boundary time, not a claim about the exact instruction that consumed input.
- The optional `YT2S` format wraps complete frames from both directions with
  one host-monotonic timestamp assigned by the companion: output is stamped
  when a full engine frame is received; input is stamped after the client frame
  is successfully written. This records client-observed timing without clock
  synchronization with DOSBox-X.
- Session header (16 bytes): ASCII `YT2S`, `u16 format_version=1`,
  `u16 header_bytes=16`, `u64 timebase=1,000,000,000` (nanoseconds from the
  first recorded event).
- Each event is `u64 time_ns`, `u8 direction` (`0` engine-to-client, `1`
  client-to-engine), three zero reserved bytes, `u32 frame_bytes`, then a
  complete v0 frame including its 14-byte header. Events are appended in
  capture order; timestamps are nondecreasing. EOF ends the file.
- Folder-link clients write the timed `INPUT.BIN` by default. `--file` detects
  and paces a timed `STREAM.BIN`, `--replay-input` accepts a timed INPUT.BIN or
  YT2S session and schedules game INPUT frames into a fresh live link, and
  `--session` remains an alias for YT2S playback. Raw v0 fixtures are decoded
  immediately with no recoverable timing.
- `--record-session PATH` explicitly creates an optional YT2S full-duplex
  capture; TCP clients retain their automatic YT2S capture unless disabled.
  Folder-link clients do not create a separate timed recording by default,
  because their two `.BIN` transport files already carry the timestamps.
- Game files stay engine-local and are not embedded in the session; sample
  replay needs the same `WORLD.DAT` beside the client. Tests pin required data
  hashes alongside their fixtures.
- Engine time remains logical ticks; transport timestamps schedule boundary
  events, not game logic. Input order is canonical; `debug` messages are
  ignored during replay unless requested.
- `YT2S` preserves engine-emitted frames; it does not inject periodic keyframes
  or provide a seek index. Playback starts at the beginning of the session.

## Backends

| engine side | client side |
|---|---|
| `direct` (in-process) | `sdl` (window / audio / input) |
| `record` (write session) | `record` (write session) |
| `ipc` (shared memory / sockets) | `null` + `inspect` (headless, CI, debug overlays) |

### Serial link (the first live backend, 2026-10-20)

The seam build speaks the framed messages over COM1: 115200 8N1 (UART divisor 1), polled, no
interrupts, FIFO on. Writes wait for THRE and burst into the FIFO. The link is not framed by the
transport, so the message framing is also the link's synchronisation.

Out-of-band detail for this transport only:

- one received byte `'K'` (0x4B) requests a keyframe (see VIDEO above); a framed CONTROL
  `keyframe_request` does the same thing and is what the client sends (the bare byte predates the
  control layer and is kept for scripts);
- DOSBox-X's `nullmodem` bridge is a TCP socket, and it escapes every `0xFF` byte on the wire as
  `0xFF 0xFF` (telnet style, by design). The client undoes that (`client/NullModemStream.cs`);
  the obsolete serial mirror is raw; the default folder files use the timed `YT2B` wrapper.
  `transparent:1` in the `serial1` line disables
  the escaping (the value must be numeric — `transparent:true` is silently ignored), which makes
  the link raw and needs `--raw` on the client instead. A future raw transport (pty, real
  hardware) needs no such step either — `--raw`.
- a full keyframe is ~70 KB, so the emulated baud rate matters; the shipped profile uses
  `multiplier:16` (~165 KB/s measured) to keep a repaint under half a second.

## Transport: the folder pseudo-ring

The folder link is the default transport (including the Windows bundle). The
engine appends one timed record per v0 frame to `STREAM.BIN` with INT 21h
writes; the client follows the file and unwraps the records. The client appends
timed records to `INPUT.BIN`; the engine unwraps and reads them from the present
hook. The 16-byte YT2B header is rewritten on a ring wrap, followed by a fresh
keyframe on the output side. `--file STREAM.BIN` replays at the recorded pace;
`--replay-input INPUT.BIN` schedules the saved game inputs. The optional YT2S
full-duplex session remains available for TCP capture and compatibility.

Each file is a pseudo-ring: at a limit (64 MiB video, 64 KiB input) the writer truncates it to zero,
rewrites the 16-byte header, and appends from offset 16. A reader detects the wrap from either
`length < position` or a length decrease since its last poll, rewinds past the header, and drops any
partial event/frame. The engine answers a video wrap with a fresh keyframe (`set_mode`, full
palette, full frame); the client keeps following the records from the beginning.

The engine polls `INPUT.BIN` with a seek to its own position before each read: DOS leaves the EOF
flag set on a file that grows under it, and a seek is the portable way to clear it.  The file
length is queried each poll, which is also the wrap check.

## Non-goals

- Remote display of the unmodified binary (dropped: emulator-hook demo, proves nothing here).
- Network multiplayer; compression research; serial/baud engineering.
