# Companion client (live display link)

The point of the seam work: run the real game, stream the display and sound
out of it, and drive a client that is a first sketch of the C# host.  Status
as of 2026-10-24: **working end-to-end** for the VIDEO and AUDIO families and
input (protocol v0; raw and run-length row codecs, 320x200x8 for the game,
640x480x8 for the VBE intro, OPL register writes synthesised in the client)
over the folder link (`STREAM.BIN`/`INPUT.BIN`; the serial link is kept for
fun).

The seam build is not byte-identical by design; the oracle build stays byte-perfect
(`sh engine/asm/swreg/tools/verify.sh` -> `SWREG.EXE: OK` + `IDENTICAL`). The seam itself is described in
`docs/stream.md`; the boundary contract is `docs/protocol.md`.

## Pieces

| piece | path | what it is |
|---|---|---|
| engine side | `engine/asm/swreg/src/seam.asm`, built by `engine/asm/swreg/tools/build.sh --variant file` | the integration surface: hooks in the game source, present/keyframe/palette logic, `STREAM.BIN`/`INPUT.BIN` backend |
| bridge | the game's working directory | the engine appends `STREAM.BIN` and reads `INPUT.BIN`; a local or shared folder |
| client | `client/` (C#, `net11.0`) | SDL3 window via P/Invoke (no NuGet), decoder, renderer, stats, recorder/replay |
| gates | `tests/replay`, `tests/audio`, `tests/seam-*`, `tests/xbrz`, `tests/window` | decode, OPL synthesis, codecs/pacing, filter and window |

The window and taskbar icon is the game's own 32x32 `YENDOR2.BMP`: a byte-identical copy lives in
`client/Assets/yendor2.bmp`, is embedded in the assembly and set with `SDL_SetWindowIcon` right
after the window is created.  The Windows executable carries the same pixels as
`client/Assets/yendor2.ico` (`ApplicationIcon`), so Explorer and a pinned shortcut show it too.

## Quick start

    sh engine/asm/swreg/tools/build.sh --variant file  # -> build/swreg/file/ (exe + game files)
    sh client/publish-linux.sh                         # engine + client -> build/dist/yt2-linux-x64/
    sh build/dist/yt2-linux-x64/play.sh                # DOSBox-X and the client, one command

Without the bundle (same engine build, client from the tree):

    dotnet run --project client -c Release -- \
        --follow build/swreg/file/STREAM.BIN --input build/swreg/file/INPUT.BIN

The client waits for `STREAM.BIN`, so the start order does not matter; on a video ring wrap the
engine follows with a keyframe, so a client that starts late resynchronises.

Headless (no window; useful over ssh or for checks):

    dotnet build/client/bin/Release/net11.0/Yt2Client.dll \
        --follow build/swreg/file/STREAM.BIN --input build/swreg/file/INPUT.BIN --headless \
        --stats 5 --ppm build/tmp/client.ppm

Replay a recording made by the file backend (`sh tools/dosbox/stream.sh 45`):

    dotnet build/client/bin/Release/net11.0/Yt2Client.dll --file STREAM.BIN --headless --ppm out.ppm

The obsolete serial link (kept for fun):

    sh engine/asm/swreg/tools/build.sh --variant serial  # -> build/swreg/serial/
    client/server.sh                                     # DOSBox-X window, COM1 -> TCP :2324
    dotnet run --project client -c Release -- --tcp 127.0.0.1:2324

## Windows build

    sh client/publish-win.sh                 # -> build/dist/yt2-win-x64(.zip)

Publishes a self-contained win-x64 single-file `client/Yt2Client.exe` (no .NET install needed on
the target), puts the pinned SDL3 runtime next to it (SDL 3.4.18 win32-x64; the archive and the
DLL are both sha256-checked, `--sdl3 PATH` uses a local DLL instead), and stages the game side:
`server/game` (the seam build) and `server/dosbox-x.conf`, plus `sample/stream.bin`,
`smoke-test.bat`, `client-start.bat`, `server-start.bat` and `play.bat`.  The launchers live in
the bundle root; `play.bat` starts the server in its own window, then the client, and stops the
server when the client exits.  `--server-dir` selects another seam build (default
`build/swreg/file`), `--out` the output root, `--no-zip` skips the archive.

`client/Sdl.cs` loads `SDL3.dll` on Windows, `libSDL3.so.0`/`libSDL3.so` on Linux and
`libSDL3.dylib` on macOS, so the same sources run on every platform; only the runtime has to be
where the loader looks. Note the bundle cannot be executed where it is built (from Linux): the
smoke test is the first thing to run on the Windows side.

## Linux build

    sh client/publish-linux.sh # -> build/dist/yt2-linux-x64.tar.gz

Same bundle shape as the Windows one: a self-contained linux-x64 single-file `client/Yt2Client`
with `libSDL3.so.0` next to it, `server/game` (the file-backend seam build) and
`server/dosbox-x.conf`, plus `sample/stream.bin`, `smoke-test.sh`, `client-start.sh`,
`server-start.sh` and `play.sh` (starts the server, then the client, and stops it again).
`--framework` publishes framework-dependent (needs .NET 11 on the target),
`--server-dir`/`--out`/`--no-tar` behave as on Windows.

## The window

The window is created before the link is opened, so a client that cannot reach DOSBox-X still
shows something, and the first line reports what the window system actually made:

    client: window 960x600 at 32,84 (Windows / Direct3D 11)

The position and size come from SDL, not from the requested values; the names are SDL's driver
and renderer (`windows`/`x11`/`cocoa`, `direct3d11`/`opengl`/`software`) with their product
spelling. If the GPU path is unavailable the client retries with the software renderer, and if
the window system still reports the window hidden, it calls `SDL_ShowWindow` and says so.
`--hold` keeps the window open after a recording ends (the Windows `smoke-test.bat` uses it).

The window is live while the link comes up: the connect loop pumps SDL
between retries, so it can be moved, keeps painting the black first frame and
closes normally even when no server is listening.  Closing the window (or
`Ctrl+Q`) cancels the wait and the client exits quietly.  With a live link the
pointer stays visible and is never captured: it drives the game while it is
over the window and belongs to the desktop again as soon as it leaves.  The
game never sees `Ctrl+Q`.

A lost link is not fatal: when DOSBox exits the reader sees the closed socket,
the client keeps the last frame and prints `client: link lost; waiting for the
server`, then reconnects when the server returns.  A windowed client retries
until it is closed (`--connect-timeout` applies to headless runs only), and a
write that races a dying socket is swallowed and marks the writer dead, so a
keystroke can never take the process down.  Headless runs still end with the
link, which keeps the gates short.

One trap is worth spelling out, because it is invisible to every pixel gate: SDL3 windows are
shown unless `SDL_WINDOW_HIDDEN` (0x8) is passed, and `SDL_WINDOW_RESIZABLE` is 0x20. Passing
0x8 as "resizable" yields a window that is created, gets a renderer, presents frames — and is
never mapped and never appears in the taskbar, which looks exactly like "the client has no
window". Every offline gate renders offscreen and stays green. `tests/window/run.sh` runs the
client on a private X server and requires `Map State: IsViewable` plus the first presented
frame; it is the guard for this class of bug.

## Folder link (`STREAM.BIN` + `INPUT.BIN`)

The serial/UART path tops out at ~180 KiB/s in DOSBox-X, so the bundle uses a shared folder
instead: two files, one per direction, in the engine's working directory (`server\game` in the
Windows bundle).

| direction | file | writer | reader |
|---|---|---|---|
| video/control/audio | `STREAM.BIN` | engine (INT 21h append) | client `--follow` |
| input | `INPUT.BIN` | client (append, flushed per message) | engine (present poll) |

Both files are pseudo-rings.  At a size limit the writer truncates the file to zero and continues
from offset 0 (8 MiB for video, 64 KiB for input); the reader notices the shrink and rewinds.  On a
video wrap the engine immediately follows with `set_mode`, a full palette and a full frame, so a
client that restarts at offset 0 is back in sync.  The client checks both the read position against
the length and a length decrease between polls, so it catches the wrap even when new data has
already regrown the file past its old length.

    sh engine/asm/swreg/tools/build.sh --variant file      # build/swreg/file, the folder build
    build/client/bin/Release/net11.0/Yt2Client --follow build/swreg/file/STREAM.BIN \
        --input build/swreg/file/INPUT.BIN

The engine opens `INPUT.BIN` lazily (the client may start later) and seeks to its own read position
before every chunk, which also clears the DOS EOF flag that a growing file would otherwise leave
set.  `--input` defaults to `INPUT.BIN` next to the followed file.  Losing the link still works:
the client keeps following the file and the engine rewrites it when DOSBox restarts.

The directory can be local or shared, so the folder link crosses a machine boundary as well - a
mounted or synced directory needs no network protocol of its own.  The serial transport
(`--tcp`, COM1 over a nullmodem) that the client could also speak is **obsolete** and kept only
for fun.

## Protocol behaviour over the link

- The seam emits `set_mode`, then `rect` / `palette` diffs, and closes every send pass with
  `frame_end` (so a pass with only palette changes still presents).
- A mode-13h picture that moved as a whole — the game pans the camera past the gate in the opening
  scene — arrives as one VIDEO `scroll(dy)` message followed by the rows that slid in, and
  any row that did not move with the picture.  The client shifts its replica when the message
  arrives (before the frame is presented) and marks the whole frame dirty, because every row
  moved: the filter re-scales from scratch, which costs 3 ms at 320x200.  This is what keeps the
  pan at the game's own picture rate: as row rects it was 168 messages and 22 KB per picture and
  the seam managed 11 pictures/s, as a scroll it is one 2-byte message and one row.
- On connect the client sends CONTROL `keyframe_request` and `hello` (`--no-keyframe` drops the
  former) and repeats each once a second until it is answered: the `hello` until the engine's
  `hello(role 0, version 0)` arrives, the keyframe until a `set_mode` arrives.  Retrying the
  keyframe matters because the two frames are 32 bytes together and the engine's UART FIFO holds
  16: the `hello` used to survive while the keyframe behind it was dropped, leaving the client on
  the old screen size for the rest of the session.  The 15-byte keyframe frame now goes first.
- An answered keyframe is `set_mode`, the full 256-entry palette and a full-frame pass (200
  one-line rects for mode 13h, more for VBE).
- A `rect` that is intact but does not fit the client's current mode is counted in `bad_rects`
  and makes the client ask for a keyframe (at most once a second, one console line).  It does not
  resynchronise the stream: the frame was well formed, the engine is simply drawing a mode the
  client has not been told about yet.
- Key presses are forwarded as INPUT `key(down, scancode, ascii)`; the engine puts them in the BIOS
  keyboard ring, so the game's own input code sees them unchanged.  Releases are not sent (DOS has
  no key-up).  `--key SECONDS:SCAN:ASCII` scripts one press for tests and demos.
- DOSBox-X drops whatever the guest wrote before a client connected, so a client joining a running
  game starts mid-message. The client resynchronises on the `'YT'` magic; the payload-length
  sanity check plus the decode checks make a false boundary practically impossible.
- Palette traffic is staged at the source and coalesced engine-side: each palette hook copies the
  exact bytes the game is about to write (a fade step, a BIOS block load or the intro loader's
  768-byte table) into the engine's pending palette, and a present pass sends the span from the
  first to the last entry that differs from the palette the client already has. If no staged write
  is pending, a game-context pass samples the DAC after a game palette write has established the
  current mode's palette, catching writes outside the hooks. Tick-ISR passes also sample the DAC
  when no staged write is pending, but only after two quiet ticks since the last palette hook and
  never while a hook is active; this avoids interrupting the game's DAC write. Mode entry invalidates
  that baseline because BIOS silently reloads its default palette; a staged palette survives mode
  detection only when that detection happened inside its hook. The DAC is also read for a forced
  keyframe send, where the client's picture has been cleared anyway. A mode
  set arms `seam_palhold`: the next four passes send no palette and hold the keyframe rows while the
  game rewrites the DAC. Palette messages never close a frame on their
  own: normally they are sent with the rows and `frame_end` follows, so the palette is not presented
  on top of the previous picture. A held transition can carry later palette-only updates across
  present passes; the client presents only when the engine closes the frame. A scene-sized jump with
  rows, or any palette change paired with at least 160 changed rows in mode 13h, holds that frame for
  one pass. A near-full repaint stays open for one bounded extra present pass, letting the menu's
  palette sequence settle without delaying ordinary fades. The intro's fades run with interrupts masked,
  so no tick pass happens between their steps; a palette hook therefore runs a pass itself
  (rate-limited by the same BIOS-tick gate), which is what keeps the new picture and its fade in
  step.  When that pass is rate-limited the staged bytes stay for the next pass, and a tick pass
  may send them too (staged bytes only, no DAC access): otherwise a screen drawn while the game
  sits in a busy wait — the title after the intro, the story screen after the logos — would go out
  with the previous palette.  A big palette jump (more than 24 units on any channel) with no rows
  in its pass is held for up to 12 passes, until the rows that belong to it arrive.  A big jump
  that arrives with rows holds its frame for one grace pass, so a fast fade-out that follows in
  the same tick (the title blit, then the game's saved default palette faded to black) replaces the
  jump before the frame reaches the screen; a held frame takes the next staged palette
  immediately, bypassing the quiet-jump hold.  After a mode set the keyframe rows wait out the
  four-pass palette hold (`seam_palhold`): sending them earlier presented them with the previous
  palette — one garbage-coloured frame.
- A video pass runs at most three times per BIOS tick, driven from the game's INT 1Ch tick
  handler and from the input-poll hooks.  The input-poll hooks alone were not enough: the intro
  draws animated frames without polling input between them, so the stream only saw the final
  frame of each transition (the client appeared to run at under 1 FPS while DOSBox drew 15-45
  changes/s).  Presenting from the tick follows the game's real animation.  One pass per tick
  turned out to be the limit that made a camera pan (one screen row per picture) step: the game
  advances the pan once per BIOS tick, and the client saw 16-17 of those 18.2 pictures a second.
  The gate now allows `SEAM_PASSES_PER_TICK` = 3 passes per tick (55/s), which is enough for the
  game's own frame rate and for the intro's fades (36-53 pictures a second measured): a poll loop
  still cannot flood the screen, and the game keeps its speed.  A finer clock
  inside the tick is not available: SBFMDRV reprograms PIT channel 0 to ~64 Hz (a 15.6 ms reload,
  see `docs/sbfmdrv.md`), so its counter cannot time a pass interval.  The row scan is a few
  milliseconds; the VBE pass moves 32 KB of rows per visit, which keeps a full 640x480 keyframe
  under a second.
- The client presents only on `frame_end`.  Presenting on every rect and palette message meant
  converting the whole framebuffer once per row (about 200 times per frame) and showing half-drawn
  frames: the window stuttered, and rows drawn before their palette arrived flickered.  One
  snapshot per frame keeps the client at a few percent CPU and every shown frame consistent.
  Window resize and expose events re-present the last frame: the renderer's back buffer is
  invalid after them, and a static game would otherwise leave a black window.
- The engine commits `STREAM.BIN` after every flush (`INT 21h` AH=68h): DOSBox buffers the
  host-side file, so without it the reader sees the stream in 4 KB bursts and the last frame of
  a burst never arrives.  Measured with `--keyframe-every 0.05` (a full 320x200 frame each time),
  the link sustains 19-20 frames/s, so the commit costs nothing.
- The video ring is 64 MiB (`SEAM_VIDEO_LIMIT`): at full-frame rates a smaller ring wrapped every
  few seconds, and each wrap rewinds the client and restarts from a keyframe — a periodic flash.
- The engine sends a debug control message at start-up with its build marker; the client prints
  it as `engine debug: ...`, which tells a log apart from an older engine.

## The legacy serial link (kept for fun)

DOSBox-X profile (shipped as `tools/dosbox/dosbox-x-client.conf`):

    [serial]
    serial1 = nullmodem port:2324 multiplier:16

- **Server mode is the default.** `nullmodem port:2324` listens; `server:<host>` means *client*
  mode (connect out to `<host>`) — easy to get backwards, and a wrong form fails silently: no
  listener is created and `dosbox-x -printconf` only prints the config path, not the config.
- **`multiplier:N` scales the emulated baud rate** without the guest knowing. Measured with a
  polled writer: `1` -> 11,465 B/s (exactly 115200/10), `4` -> 46,004 B/s, `16` -> ~165 KB/s.
  A full keyframe is ~70 KB, so 16 keeps a full repaint under half a second.
- **The link doubles every `0xFF` byte** (telnet-style escape, `CNullModem::transmitByte`):
  `0xFF` arrives as `0xFF 0xFF`. `client/NullModemStream.cs` undoes it on receive and re-escapes
  on send; `--raw` turns that off for links that do not escape. This is deliberate DOSBox
  behaviour, switchable with `transparent:1` in the `serial1` line — the value must be numeric
  (`transparent:true` is *silently ignored*). The shipped profile keeps the escaping default so
  the client works out of the box against any stock nullmodem line; with `transparent:1` the link
  is raw and the client needs `--raw` (both combinations are verified byte-exact). Recordings from
  the file backend are raw.
- The seam polls THRE and writes in 16-byte FIFO bursts, with a bounded wait: a dead link makes it
  drop the rest of that message instead of freezing the game.

## Scaling

The engine's frame is 320x200 (or 640x480 for the VBE screens) and the window is
`--scale` times as large.  By default the graphics driver does the scaling with the nearest
neighbour, so every pixel stays a crisp block.

`--filter xbrz` scales the frame in the client instead, with the xBRZ filter ("scale by rules",
Zenju's pixel-art scaler).  It is the advanced form of the algorithm: a 4x4 kernel decides for
every corner of a pixel whether it has to be blended and how strongly, and only those corners are
blended, which keeps the small features of the original art and smooths the rest.  The scale
factor is the window scale (2 to 6).

The client's implementation is an independent one of the published algorithm, and its arithmetic
follows the reference implementation step for step: `tests/xbrz/run.sh` replays the sample
recording with the filter and compares the filtered frame with the reference's output, byte for
byte.

The filter scales only the rows that changed (with the row above and below them, because a
blend looks two rows up and down), so a frame costs about 5 ms at 3x instead of scaling the
whole picture every time.

## Audio

The seam streams the resident SBFMDRV's OPL register writes and the PC
speaker's gate over the AUDIO family (`docs/protocol.md`); the client is the
FM chip and the speaker.  `client/Opl2.cs` is
an independent YM3812 core: nine two-operator channels, FM or additive
connection, feedback, the four OPL2 waveforms, the ADSR envelope with the
YMF278B rate tables, key scaling, total level and KSL, and tremolo/vibrato.
`client/Opl.cs` keeps one 256-byte register file per FM port and feeds one
chip per port; `client/SdlAudio.cs` opens an SDL3 signed-16 stereo stream at
44100 Hz and keeps about 100 ms queued.  Rendering runs on the main thread, so
the reader thread only applies register writes.

The client opens the device unless `--headless` or `--no-audio` is given; a
missing device is reported and the picture continues without sound.  `--dro
PATH [--wav PATH]` renders a DOSBox-X raw OPL capture (DRO 2.0) through the
same core, which is how the synth is checked against the reference WAV without
running the game.  `--file rec.bin --wav out.wav` renders a protocol
recording's AUDIO family offline, with the write timestamps, so a folder-link
capture can be checked (or heard) without the engine.

The bundled DOSBox-X profiles set `nosound = true`: the emulated Sound
Blaster/OPL still runs (the game and its driver need it, and the seam reads
its writes), but DOSBox-X does not play the host audio, so the client is the
only output and there is no second, delayed copy.  To hear the emulator
instead, set `nosound = false` in the bundle's `server/dosbox-x.conf` and
start the client with `--no-audio`.

Each write carries a 32-bit PIT stamp (1,193,182 Hz; `docs/protocol.md`), and
`client/Opl.cs` schedules it on the render timeline: the first timed write
anchors at the render front, later stamps advance the schedule by their
difference, and a write that maps behind the front is played at the front.
A batch that carries several SBFMDRV ticks therefore keeps their ~10.4 ms
spacing instead of collapsing into one instant.  The client keeps about
100 ms rendered ahead of the device, so the whole track is heard that much
after the engine's events; that is queue latency, not a per-write error.
The stream's stamps are non-decreasing (the seam shifts the timeline when the
driver resets its phase at a music start); the 32-bit count rolls over about
hourly and the client compares deltas modulo 2^32.

The PC speaker rides the same timeline.  `fn_SpeakerBeep` calls the seam's
`fn_SeamSpeakerOn`/`fn_SeamSpeakerOff` around its port 61h edges, and
`client/Opl.cs` renders a square wave at `1,193,182/divisor` (30% FS) between
them, mixed with the FM output.  The beep's gate is independent of the music
state (`audio_ctrl` mirrors the sequencer), so it is heard even when no track
is playing.

SBFMDRV is a music driver: the game's SFX path (`fn_PlaySFX`) normally needs
`v_SoundState` bit 3 and the far entry (`v_SoundEntry`), which SBFMDRV 1.34
does not provide.  The data path is still there: WORLD.DAT carries 80
Creative Voice File records (8-bit unsigned PCM, 4–15 kHz, 0.1–5 s), indexed
by the game's SFX pointer table at `DS:0CF51h` (`v_5160` = 80 entries).  The
seam reports every `fn_PlaySFX` call instead: it replaces the first six-byte
`TEST` with a far call plus nop, sends `AUDIO sample(id,offset,pit)` (type 5),
and the client
reads the VOC record at `offset` from the WORLD.DAT next to `STREAM.BIN`
(`--world` overrides the lookup).  The client resamples the record onto the
same PIT timeline as the FM writes and mixes it with the OPL output, so the
samples and music stay in step.  Up to eight records mix at once; a ninth
drops the oldest.

Effect 3 is special: in the reference configuration the game falls back to
`fn_SpeakerBeep` (PIT channel 2), whose gate edges travel as AUDIO type 4 and
render as a 30% square wave.  The seam's sample message for id 3 is queued
first; the client drops the following on/off pair when it has the sample, so
the real VOC replaces the beep.  Without WORLD.DAT the bank is unavailable
and the beep still plays, which keeps music-only recordings useful.  The
game-side beep caller is the rejected-selection path (`fn_04932` passes
effect id 3).  A temporary one-shot call at `fn_SeamPresentMusic` confirmed
the live beep transport: a 40 s run carried one on/off pair, divisor 662,
219.7 ms apart (262,140 PIT counts); the patch was reverted before the
commit.  The sample path is pinned offline against the real effect records
(`09FFE6h` for id 3, `0A076Ah` for id 5) and was exercised by the keyed
capture below; the remaining check is audible playback on a machine with a
PCM device.

## Client options
| option | meaning |
|---|---|
| `--tcp [host:]port` | connect to the nullmodem socket (default 127.0.0.1:2324); **obsolete**, kept for fun |
| `--file PATH` | replay a recording instead of connecting |
| `--follow PATH` | follow the engine's growing `STREAM.BIN` (folder link) |
| `--input PATH` | input file for `--follow` (default: `INPUT.BIN` next to it) |
| `--world PATH` | WORLD.DAT for the VOC effects (default: next to `--follow`/`--file`, then `server/game/WORLD.DAT` next to the client) |
| `--headless` | no window: decode, report, `--ppm`/`--dump` only |
| `--scale N` | window scale (default 3) |
| `--filter NAME` | `nearest` (default) or `xbrz`: the pixel art is scaled by the client with the xBRZ filter instead of block by block |
| `--stats SECONDS` | progress line interval, 0 disables (default 5); lines end with `fps=` |
| `--connect-timeout N` | headless: give up connecting after N seconds (default 30) |
| `--no-keyframe` | do not ask for a keyframe on connect |
| `--keyframe-every S` | ask for a full frame every S seconds (diagnostics) |
| `--hold` | keep the window open after a recording ends |
| `--no-audio` | do not open an audio device |
| `--dro PATH` | render a DOSBox-X raw OPL capture (offline test) |
| `--wav PATH` | with `--dro` or `--file`: render the audio to this WAV instead of opening a device |
| `--ppm PATH` | write the last frame to a PPM file |
| `--dump PATH` | record every byte the decoder saw to PATH |
| `--raw` | do not unescape the nullmodem `0xFF` doubling (obsolete) |
| `--key S:SCAN:ASCII` | press a key S seconds after start (repeatable, live link) |

Keys go to the game (arrows and function keys included, as extended keys).
`Esc` is forwarded too, since the game needs it: quit with the window close
button or `Ctrl+Q`.

## Audio verification (2026-10-24)

- `tests/audio/run.sh`: the committed `music-walk.dro` fixture renders to
  12.66 s, peak 41.2% FS, rms 2261 (the paired DOSBox-X WAV: 41.8% FS, rms
  2310); the Goertzel checks pin the pitches of the first bars; the same DRO
  encoded as `opl_batch_timed` and replayed with `--file --wav` is
  sample-identical to the direct render; the dummy SDL device opens and the
  client pushes 4,410 frames from `sample-audio.bin`.
- Live folder run (2026-10-24): 20 s, 240 timed batches / 1,699 writes / 4
  controls, no backwards stamp, 96 Hz tick spacing (12,428 PIT counts).
- PC-speaker gate: `sample-audio.bin`'s on/off pair renders a 220 ms 1800 Hz
  square wave at 30% FS; `tests/audio/run.sh` checks the render length, the
  level, the pitch and that the tone stays inside the gate window.  The final
  hook shape was also confirmed live: a 40 s run with a temporary one-shot
  call carried one on/off pair (divisor 662, 262,140 counts = 219.7 ms apart)
  and the game ran normally.
- VOC effects: the same gate renders the real effect 3 (`09FFE6h`, 0.20 s)
  with its beep pair, effect 3 alone, effect 5 (`0A076Ah`, 1 s at 345 Hz)
  alone, and the beep alone.  The effect-3 stream is sample-identical to the
  sample-only render over the sample-only length, so the beep pair was
  dropped; a control stream with only the beep still renders the 1800 Hz
  square wave.
  Effect 5 renders 2.00 s with rms 1488 in the record window, and its 345 Hz
  tone dominates 1800 Hz.
- Live keyed capture (2026-10-25): `STREAM_KEYS="62:Return 63:space ..."`
  with `tools/dosbox/stream.sh 95` advanced the intro to the story scene.
  The recording carried 9 `sample` messages (ids 16–25, offsets matching the
  game's `0CF51h` table), and `--file --wav --world _ref/orig-bin/WORLD.DAT`
  rendered 95.51 s with 9/9 played and 0 unknown.  The sample windows are
  rms 2.2k–4.7k against ~1.1k between lines.  That scene never takes the
  effect-3 fallback, so the recording has no speaker edges.

## Verification (2026-10-20)

- File backend, no input: 483 messages / 150,360 bytes in `STREAM.BIN` (the first is the engine
  `hello`); the Python replay sees 41 frames, 402 rects, 38 palette messages, 1 control message.
- Live link, tee build (one session, both backends): the client received 685 messages /
  219,873 bytes (after a scripted Return at 20 s) and `tools/stream/compare_streams.py` reports
  **every received message byte-identical**
  to the seam's own `STREAM.BIN`; the four missing messages were sent before it connected. The
  client's final frame is pixel-identical (0 differing index or palette bytes) to an independent
  decode of that same `STREAM.BIN`.
- Replay of an earlier recording: the C# render is byte-identical to the Python decoder's PPM
  (`cmp`).
- Window gate (`tests/window/run.sh`): with the released flags the client's window is mapped at
  960x600 and presents a frame; with the old flags (0x8) the same gate fails with
  `Map State: IsUnMapped` while the replay gate stays green — the regression is only visible
  here.
- Connect without a server: the window is mapped at 960x600 and `Ctrl+Q` during
  the retry loop exits the client immediately.  Before, `Connect` ran on the
  main thread after the window was created, so the window was frozen for the
  whole `--connect-timeout` and the OS called it "not responding".
- A/B against the pre-refactor stream build (before palette coalescing): final frames are
  pixel-identical; mid-fade states differ only by the two-tick pacing.
- Server death and return: killing DOSBox mid-session left the client running
  with `link lost; waiting for the server`; the restarted server was picked up
  as a second session (fresh handshake, `set_mode`, frames) without touching
  the client.  A bare TCP reset is survived the same way; headless runs still
  end when the link does.

## Debugging the link

- `--dump PATH` gives the bytes the decoder saw (after the unescape). Compare with an expected
  stream:

      python3 tools/stream/compare_streams.py build/tmp/e2e7/game/STREAM.BIN client.dump

- Tee build — the seam writes `STREAM.BIN` *and* mirrors every message to COM1, so one run
  produces both sides:

      sh engine/asm/swreg/tools/build.sh --variant tee

- Log build — `SEAMLOG.TXT` in the game directory gets packed records (palette hook calls,
  palette send spans, present pacing, every message with its length and sequence):

      sh engine/asm/swreg/tools/build.sh --variant log

  Read it with `python3 -c` around `engine/asm/swreg/tools/load.py`; records are 8 bytes: tag, value1,
  value2, 0.  Current tags: 1-3 DAC writes (`sub_SeamPalette*`), 4 palette send span, 5 frame_end,
  6 present pacing skip, 7 present pass, 8 received message (family, type), 9 sent message
  (sequence, length), 10 mouse event handed to the game (mask, handler), 11 bytes read from the
  UART in one poll (count, staged), 12 end of a staged message (length; also a dropped frame),
  13 INT 33h `0Ch` (handler, mask), 14 INT 33h `00h` (handler, caller segment), 15 the shim's
  saved INT 33h vector, 17 a byte parked while transmitting.

## Mouse input

The game talks to INT 33h, so the engine installs a shim in `fn_SeamInit` — before
`fn_MouseInit` probes the vector — and answers only what the game uses (`00h`, `03h`, `04h`,
`0Bh`, `0Ch`, `0Fh`); anything else is chained to the driver that was there before (DOSBox-X's
own).  `0Ch` stores the game's own event handler, and every client `mouse_abs`/`mouse_button`
message updates the shim's counters and position and then calls that handler exactly as a driver
would (AX mask, BX buttons, CX/DX position).  `mouse_abs` carries the exact target in game pixels:
the shim sets the motion counters to `target - current`, so the game's own `fn_MouseMoveBy` clamps
and lands on the target and repeated updates cannot accumulate drift.  The game's software cursor
therefore follows the client, and clicks latch at the position the cursor is drawn.
A reset (`00h`) after the game installed its handler means it is leaving: the shim restores the
previous vector and goes quiet.

Client side: the window keeps the normal desktop pointer (no relative mode, no
capture, cursor visible).  While the pointer is over the focused window, SDL
reports its absolute position; the client derives the game viewport from the
live window size (the renderer uses integer scaling with letterboxing) and
sends it as `mouse_abs(x, y)` in game pixels.  Using the live size, not the
`--scale` the window was created with, keeps the mapping exact after a resize
or maximise, in 320x200 and 640x480 alike.  Leaving the window or losing focus
simply stops the updates; re-entering sends the position again, so the cursor
cannot drift.
Scripted hooks for tests and demos (the relative `mouse_move` message is kept
for recordings):

    --mouse S:DX:DY       move DX,DY game pixels at S seconds
    --click S:BUTTON:0|1  button edge (0 left, 1 middle, 2 right)

Known limitation: this harness's nullmodem link drops host-to-guest bytes whenever the game is
not reading (the reason the client retries its `hello`).  A client that connects early and polls
sees input normally, but a single input message sent while the engine is busy can be lost; keyboard
and mouse share the path, and scripted input is easiest to observe early in a run.

## Not here yet

Further codecs (the row runs of codec 1 are in; LZ is not) and session
headers for recordings.  VBE is supported for the one mode the game uses (640x480x8,
the intro); the seam ignores any other VBE mode.  The client is a development
harness: one window and the live folder link.
