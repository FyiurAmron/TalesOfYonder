# Test fixtures

## `sample-video.bin`

A bare protocol-v0 VIDEO stream (raw codec, 320x200x8), 150,360 bytes: one no-input run of the
game with the *file* backend of the seam build (`-DSEAM -DSEAM_FILE`, `STREAM.BIN`), taken from
the same pipeline as `sh tools/dosbox/stream.sh 45`. The run is deterministic and the game idles
on the fading logo, so capture windows from 20 s to 45 s all produce this exact recording.  Since the CONTROL
layer it starts with the engine's `hello` message.

It is used by

- `tests/replay/run.sh` — the C# client's render must match the Python decoder's render byte for
  byte, so a decoder regression fails without needing DOSBox-X or the game;
- `client/publish-win.sh` — the Windows bundle ships it as `sample/stream.bin`, so
  `smoke-test.bat` verifies the published binary on the target machine.

Decode stats: `frames=41 rects=402 palette=38 control=1 records=483 bytes=150360`. Regenerate any time with
a fresh file-backend run; the clients accept any such recording, the numbers above belong to this
one.

## `sample-vbe.bin`

The same pipeline (file backend, no input) truncated at the end of the VBE intro, 123,257 bytes:
`set_mode(320,200,8)` for the "SW Games" logo, then `set_mode(640,480,8)` and the Spectrum
Pacific intro at 640x480, ending before the game returns to mode 13h. It exercises the second
engine mode (reallocating replica and texture, 640-wide rows, palette swaps) and makes
`tests/replay/run.sh` cover both decoders at both sizes.

Decode stats: `frames=11 rects=184 palette=2 control=1 records=200 bytes=123257`, final mode
`640x480x8`.
