# Test fixtures

## `sample-video.bin`

A bare protocol-v0 VIDEO stream (raw codec, 320x200x8), 150,360 bytes: one no-input run of the
game with the *file* backend of the seam build (`-DSEAM -DSEAM_FILE`, `OUTPUT.BIN`), taken from
the same pipeline as `sh tools/dosbox/stream.sh 45`. The run is deterministic and the game idles
on the fading logo, so capture windows from 20 s to 45 s all produce this exact recording.  Since the CONTROL
layer it starts with the engine's `hello` message.

It is used by

- `tests/replay/run.sh` — the C# client's render must match the Python decoder's render byte for
  byte, so a decoder regression fails without needing DOSBox-X or the game;
- `tests/window/run.sh` — checks that the client shows a frame while holding the
  window open.

Decode stats: `frames=41 rects=402 palette=38 control=1 records=483 bytes=150360`.
Regenerate with a fresh file-backend run; the clients accept any such recording,
the numbers above belong to this one.

## `sample-timed-stream.bin`

A timed `YT2B` engine-to-client stream built deterministically from
`sample-video.bin`. Each original v0 frame is preserved; `frame_end` advances
the PIT clock by 1/24 second. It is the timed `.BIN` sample shipped by both
bundles and exercises paced playback without requiring a separate recording
container.

Regenerate or verify it with:

    python3 tests/session/make_sample.py
    python3 tests/session/make_sample.py --check

`tests/replay/run.sh` compares its C# and Python renders just like the raw
fixtures above.

## `sample-session.yt2s`

A 12-second full-duplex capture from the asm folder-link client. It remains as
a compatibility fixture for the optional `YT2S` wrapper: engine output and
client input frames share one monotonic timestamp timeline. The bundles now
ship `sample-timed-stream.bin` instead; `--file` continues to detect the
`YT2S` header automatically.

It is 190,623 bytes / 12.84 seconds and decodes to 159 frames, 783 rects, 157
palette messages, 3 control messages and 1,288 timed OPL writes. The capture
includes the handshake and a scripted Return at 7 seconds.

## `sample-vbe.bin`

The same pipeline (file backend, no input) truncated at the end of the VBE intro, 123,257 bytes:
`set_mode(320,200,8)` for the "SW Games" logo, then `set_mode(640,480,8)` and the Spectrum
Pacific intro at 640x480, ending before the game returns to mode 13h. It exercises the second
engine mode (reallocating replica and texture, 640-wide rows, palette swaps) and makes
`tests/replay/run.sh` cover both decoders at both sizes.

Decode stats: `frames=11 rects=184 palette=2 control=1 records=200 bytes=123257`, final mode
`640x480x8`.

## `sample-video-rle.bin`

A file-backend recording with the seam's run-length row codec (codec 1), 399,860 bytes, cut at a
record boundary: the 640x480 intro, the mode switch to 320x200 and the start of the game, so both
engine modes and the RLE decoder are covered. Every rect row here is run-length coded; the clip
carries 28,632 rects in 400 KB, which the same rows would not fit in raw (they were 64 bytes per
320-pixel row of flat art and up to 647 per noisy row).

Decode stats: `frames=182 rects=2681 palette=141 control=3 records=3011 bytes=399860` (the client
merges rows, the Python decoder counts records), final mode `320x200x8`. `tests/replay/run.sh`
decodes it with both implementations and compares the rendered PPMs, so the asm encoder, the C#
decoder and the Python reference cannot drift apart unnoticed.

## `sample-audio.bin`

A small synthetic protocol-v0 stream (4,843 bytes, 11 records) that interleaves the AUDIO family
with VIDEO: `hello`, `set_mode(320,200,8)`, a full palette, two 320x6 rects and two `frame_end`s,
with one `opl_write`, one `opl_batch`, one `opl_batch_timed` and one `audio_ctrl` between them.
`tests/replay/run.sh` renders it through the C# client and the Python decoder and compares the
PPMs, so the picture path must be unaffected by the audio messages while both sides parse the same
layout (family 2, `docs/protocol.md`). Regenerate with
`python3 tests/replay/mkaudio.py tests/data/sample-audio.bin`.

Decode stats: `frames=2 rects=2 palette=1 control=1 audio=7w/2b/1c records=11 bytes=4843`.

## `music-walk.dro`

A DOSBox-X raw OPL (DRO 2.0) capture of the resident SBFMDRV playing one
`WORLD.DAT` music block (2,318 bytes, 808 register writes, 161 note-ons,
11.656 s).  It is the reference input for the client's OPL2 core:
`client --dro music-walk.dro --wav ...` renders it, and `tests/audio/run.sh`
checks the duration, level and pitches.  Regenerate with

    MUSIC_PLAY=1 MUSIC_CAP=dro sh engine/asm/sbfmdrv/test/run.sh music

and copy `capture/music_000.dro` (the capture is a test artifact, not a
recording of the game's own output).  `tests/replay/mkdrostream.py` turns the
DRO into a protocol recording with `opl_batch_timed`; `tests/audio/run.sh`
replays it and requires a byte-identical WAV, which pins the stamp layout and
the client's scheduler to the direct render.

## `sample-pan-rows.bin` / `sample-pan-scroll.bin`

The same twelve pictures — a 320x200 scene sliding up six rows and back down
six — recorded twice: first as changed rows, then as one `scroll(dy)` message
per frame plus the row that slides in. 18,887 bytes against 202,587 for the
row form, which is the point of the message: a panning camera used to cost a
row message per screen row. `tests/seam-scroll/run.sh` generates and checks
them (the committed files must be what `mkstream.py` builds, and both must
decode to identical frames), and `tests/replay/run.sh` renders both with the
C# client and the Python decoder and compares the PPMs.

Decode stats (the scroll recording): `frames=12 rects=211 scrolls=11 palette=1
records=236 bytes=18887`.
