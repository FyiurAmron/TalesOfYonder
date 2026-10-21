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
| client | display state (framebuffer replica, palette, mode), window/scaling/filters, OPL synthesis, input capture, debug overlays; host filesystem (final step) |

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
u8   codec           payload codec id (0=raw, 1=RLE-skip, later 2=LZSS)
u32  seq             per-direction sequence number (replay integrity)
u32  len             payload length before decoding
<len bytes payload>
```

- Codecs are stateless and side-effect-free; a receiver handles any codec it advertised.
- Unknown family/flags: ignore + `error` (forward compatibility).
- No timing/baud/hardware constraints are assumed.
- A transport that can hand over a stream mid-message (a live link joins a running engine, for
  example) may resynchronise on the magic; version/family/length sanity checks make a false
  boundary practically impossible.

## Families (v0)

| id | name | direction | messages |
|---|---|---|---|
| 1 | VIDEO | engine to client | `set_mode(w,h,palette?)`, `palette(start,data)`, `rect(x,y,w,h,payload)`, `frame_end`, `reset` |
| 2 | AUDIO | engine to client | `opl_write(port,reg,val)`, `opl_batch(payload)`, `audio_ctrl(rate,fmt,on)` (the PC-speaker beep fallback is not modelled in v0) |
| 3 | INPUT | client to engine | `key(scancode,down)`, `mouse_move(dx,dy)`, `mouse_button(btn,down)`, `quit` |
| 4 | TICK | both | `tick(n)`, `pace(ms)` (client may pace; engine never reads wall clocks) |
| 5 | CONTROL | both | `hello(role,version)`, `bye`, `error(code,text)`, `debug(channel,payload)` (product clients ignore `debug`) |
| 6 | FILES | both | `fs_open(name,mode) -> handle`, `fs_close`, `fs_read(handle,len) -> chunk|eof`, `fs_write(handle,payload)`, `fs_seek(handle,off,whence)`, `fs_findfirst(pattern) -> entries`, `fs_delete`, `fs_rename` |

### Encoding policy ("smart but trivial")

Per-family, not one-size-fits-all:

- VIDEO: only dirty rectangles; inside a rect, rows are encoded with skip/literal/repeat opcodes
  (unchanged runs become skips). Palette changes are separate messages, so palette animation costs
  a few bytes. No full-frame rebuilds unless a mode/reset forces them.
- AUDIO: OPL *register writes* travel, not PCM; synthesis is client-side. Messages are tiny, ergo
  no compression.
- INPUT/TICK/CONTROL: never compressed (overhead exceeds payload).
- FILES: bulk data, chunked to bounded message sizes; codec-eligible (RLE first, LZ later).
- Codec id is per-frame; adding a codec must never change semantics, only size.
- Rule of thumb: any codec must be implementable in ~100 lines in asm, C and C# - that is the bar
  for "shared and trivial".

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
- A `set_mode` means "start from a blank screen": the client drops its replica, reallocates at the
  new size and expands the palette again.  The engine relies on this — it does not send a full
  repaint just because the mode changed.
- A pass carries at most ~16 KB of VBE rows, so a full intro repaint is spread over a few passes
  instead of stalling the game for a frame time.
- Full-frame rects serve as keyframes; replay consumers use them for seeking (see Recordings). On
  the live serial link a **keyframe is requestable**: one received byte `'K'` (0x4B) makes the
  engine answer with `set_mode`, the full palette and a full-frame pass, which is what lets a
  client connect to an already-running game.
- Palette traffic is coalesced engine-side: hooks record writes, and a send carries the span from
  the first to the last entry that differs from the palette the client already has. The span may
  contain unchanged entries; clients must not assume it is minimal.

### Payload layouts (v0)

The first payload byte is the message type (per family). Multi-byte fields are little-endian and
unpadded. VIDEO, CONTROL and INPUT (keys) are implemented in the seam builds (see
`docs/stream.md`); AUDIO, TICK, mouse INPUT and FILES are still spec-only here:

| type | message | payload after `u8 type` |
|---|---|---|
| 0 | `set_mode` | `u16 w`, `u16 h`, `u8 bpp` |
| 1 | `palette` | `u16 start`, `u16 count`, `count*3` bytes of 6-bit DAC R,G,B |
| 2 | `rect` | `u16 x`, `u16 y`, `u16 w`, `u16 h`, `w*h` bytes of 8-bit indices (row-major) |
| 3 | `frame_end` | *(empty)* |
| 4 | `reset` | *(empty)* |

- Palette values are DAC values (0..63), not 8-bit RGB; clients expand them for display.
- Compared with the M1b spike (one full 256-entry palette message per fade step) the palette
  message is now a diff span and `frame_end` closes every pass; the seam builds record bare
  message streams (no session header yet), VIDEO family only, codec `raw`, with `h=1` rects (one
  message per changed row, at most 640 wide), in 320x200x8 for the game and 640x480x8 for the VBE
  intro.  `tests/data/sample-video.bin` covers mode 13h, `tests/data/sample-vbe.bin` the intro.
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

### Files (specified now, implemented last)

- DOS semantics live on the client: 8.3 names, case-insensitive matching, wildcard find, EOF,
  create/write/delete/rename as the game uses them.
- Bulk reads/writes are chunked (bounded messages) with explicit `eof`; the engine loops like it
  does around DOS reads.
- The engine deals in name+offset+length only; no host paths, no host quirks.

## Recordings / replay

- A recording is a two-directional framed session: header (protocol version, engine version,
  config, input map, data-file hashes) followed by direction-tagged messages.
- Self-contained mode (default once the client owns files): FS responses are included, so a replay
  needs no data files. Pinned mode (CI / fast): hashes only; files must match.
- Determinism rules: engine time = logical ticks only; input order canonical; `debug` messages are
  ignored during replay unless requested.
- Recordings interleave periodic full-frame keyframes (full-screen `rect` / `reset`) so a consumer
  can seek and render from any point.

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
  recordings written by the file backend are raw. `transparent:1` in the `serial1` line disables
  the escaping (the value must be numeric — `transparent:true` is silently ignored), which makes
  the link raw and needs `--raw` on the client instead. A future raw transport (pty, real
  hardware) needs no such step either — `--raw`.
- a full keyframe is ~70 KB, so the emulated baud rate matters; the shipped profile uses
  `multiplier:16` (~165 KB/s measured) to keep a repaint under half a second.

## Transport: the folder pseudo-ring

The Windows bundle does not use the serial link.  The engine appends framed messages to
`STREAM.BIN` with INT 21h writes; the client follows that file.  The client appends input frames
to `INPUT.BIN`; the engine reads it from the present hook.  Both are plain v0 frames, so a
recording is still just the video-file byte stream and the replay tools work unchanged.

Each file is a pseudo-ring: at a limit (8 MiB video, 64 KiB input) the writer truncates it to zero
and starts again at offset 0.  A reader detects the wrap from either `length < position` or a
length decrease since its last poll, rewinds, and drops any partial frame.  The engine answers a
video wrap with a fresh keyframe (`set_mode`, full palette, full frame), and the client answers
the wrap by simply continuing to decode from zero.

The engine polls `INPUT.BIN` with a seek to its own position before each read: DOS leaves the EOF
flag set on a file that grows under it, and a seek is the portable way to clear it.  The file
length is queried each poll, which is also the wrap check.

## Non-goals

- Remote display of the unmodified binary (dropped: emulator-hook demo, proves nothing here).
- Network multiplayer; compression research; serial/baud engineering.
