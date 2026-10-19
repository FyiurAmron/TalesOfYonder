# YT2 companion protocol — v0 (draft)

The engine core and the client/host speak one message protocol. The same messages are used for
in-process clients, IPC clients and **recordings**; the transport is a detail. Tests target this
boundary.

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

Little-endian, transport-independent, no alignment assumptions.

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

## Families (v0)

| id | name | direction | messages |
|---|---|---|---|
| 1 | VIDEO | engine to client | `set_mode(w,h,palette?)`, `palette(start,data)`, `rect(x,y,w,h,payload)`, `frame_end`, `reset` |
| 2 | AUDIO | engine to client | `opl_write(port,reg,val)`, `opl_batch(payload)`, `audio_ctrl(rate,fmt,on)` |
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
- Deltas are computed engine-side at `frame_end`: the engine compares its linear framebuffer
  against a shadow of the last presented frame and emits changed rects (implementation detail).
  VBE banking stays engine-internal: the delta engine always sees the linear image.
- Full-frame rects serve as keyframes; replay consumers use them for seeking (see Recordings).

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

## Non-goals

- Remote display of the unmodified binary (dropped: emulator-hook demo, proves nothing here).
- Network multiplayer; compression research; serial/baud engineering.
