# Tests

Nine checks pin the link and the client; eight cover the folder transport
and the client, and `tests/link` covers the obsolete serial one (kept for fun).
All are ordinary shell scripts; artifacts land under
`build/runs/<kind>/<stamp>/`.

## `tests/replay/run.sh`

Offline and hermetic (no DOSBox-X, no game): replays the recordings in
`tests/data/` through the C# client and through the Python reference decoder
(`tools/stream/replay.py`, `--ppm` in both) and compares the final frames with
`cmp`. Same framing, same palette expansion, same state, byte for byte. The
recordings cover mode 13h raw, the VBE intro raw, both modes with the
run-length row codec (codec 1), the same twelve-picture camera pan encoded
once as rows and once as `scroll` messages, a small stream that interleaves AUDIO messages
with video (`sample-audio.bin`), and the timed `.BIN` sample built from the raw fixture.

    sh tests/replay/run.sh [RECORDING]

## `tests/audio/run.sh`

The client's OPL2 synthesis and SDL3 output.  `client --dro
tests/data/music-walk.dro --wav ...` renders a DOSBox-X raw OPL capture of a
real `WORLD.DAT` music block through the same `Opl2` core the live client
uses; the WAV must be 12.7 s of signal (peak near 41% FS, RMS near 2260) and
the Goertzel checks pin the first bars' pitches.  The second half opens SDL
audio with the dummy drivers and requires the client to push frames from a
recording, which catches the binding and the stream plumbing.

    sh tests/audio/run.sh

Needs `dotnet` and SDL3 (the client already needs it for the window).

## `tests/session/run.sh`

Runs a small fake full-duplex server through the live client: optional YT2S
capture must contain both directions on a monotonic nanosecond timeline;
`--file` must pace both YT2S and the default timed `OUTPUT.BIN` format; the
Python tools must unwrap timed records; replaying either YT2S or timed
`INPUT.BIN` must write the key frame into a fresh timed input transport; and
the input clock must keep one epoch when the first server frame arrives after
the handshake. The sample `.BIN` is also checked against its deterministic
generator. This pins the timed folder-link boundary without requiring DOSBox-X.

    sh tests/session/run.sh

Needs `dotnet` and Python 3.

## `tests/window/run.sh`

Guards the one thing the pixel gates cannot see: that the client's window is
actually *on screen*. It starts the client on a private Xvfb display, replays
`tests/data/sample-video.bin` with `--hold`, and requires a window titled
`YT2 client` with `Map State: IsViewable`, the expected 960x600 geometry and at
least one presented frame. A window created with `SDL_WINDOW_HIDDEN` satisfies
every other gate (the renderer works, frames are uploaded) while the user sees
only a console, so this check is not redundant.

    sh tests/window/run.sh

Needs `Xvfb` and `xwininfo`; skips (exit 0) when either is missing.

## `tests/seam-rle/run.sh`

The row encoder of VIDEO codec 1 is assembly inside the seam, so it is tested
as assembly: the routines are extracted verbatim from `seam.asm`, built into a
.COM with a harness and run under DOSBox-X, and the encoded output is decoded
here in Python and compared with the source rows. Six 640-pixel rows cover one
long run (counts above 255), all-distinct and checkerboard rows (literal
chunks), noise, two long runs, and runs mixed with singles. This is the check
that catches an encoder regression, because `tests/replay` only replays
recordings.

    sh tests/seam-rle/run.sh

Needs `uasm`, `dosbox-x` and `Xvfb`; skips (exit 0) when one is missing.

## `tests/seam-scroll/run.sh`

The whole-screen pan detector (VIDEO `scroll`) is assembly inside the seam, so
it gets the same treatment: extracted verbatim, built into a .COM, run under
DOSBox-X, and compared against a Python model of the protocol rules. Ten
cases: pans of -1, -3, -2, +2, -5 and +8 rows (one with a row that changes on
its own), a pan past the +-8 range, a repaint, and two still pictures whose
changed rows must not be read as a shift. The same script also builds the pan
fixtures (`sample-pan-rows.bin`, `sample-pan-scroll.bin`), requires the
committed copies to match the generator, and checks that the two recordings
decode to identical pictures — the format half needs only `python3` and runs
even where DOSBox-X is missing.

    sh tests/seam-scroll/run.sh

Needs `uasm`, `dosbox-x` and `Xvfb` for the assembly half; skips that part
(exit 0) when one is missing.

## `tests/seam-pace/run.sh`

The presentation budget decides how often the game's screen goes to the
client (`SEAM_PASSES_PER_TICK` passes per BIOS tick, with forced passes
ignoring it). It is assembly inside the seam and it has broken before - an
earlier attempt paced passes off the PIT counter, which SBFMDRV has already
reprogrammed to ~64 Hz, and presented nothing at all in a tight poll loop - so
it is tested against the real BIOS tick under DOSBox-X, not against a model of
it: five calls inside one tick must give exactly `[1, 1, 1, 0, 0]`, a forced
pass on an exhausted budget must still run and clear its flag, and the first
call of the next tick must start the budget over. The harness retries a group
if a tick boundary falls inside it.

    sh tests/seam-pace/run.sh

Needs `uasm`, `dosbox-x` and `Xvfb`; skips (exit 0) when one is missing.

## `tests/xbrz/run.sh`

The xBRZ filter must produce exactly the pixels the reference implementation
produces.  The client replays `tests/data/sample-video.bin` with
`--filter xbrz`, which scales the last frame by the window scale (3) and
writes the filtered pixels; the check compares them with
`tests/data/sample-video-xbrz3.bin`, the reference implementation's output for
the same frame, byte for byte.

    sh tests/xbrz/run.sh

Needs `dotnet` only.

## `tests/link/run.sh`

**Obsolete** - the folder link is the product transport; this drives the COM1/nullmodem link,
kept for fun (and because it is the only socket-level live check). Live: builds the tee variant
of the engine (`-DSEAM -DSEAM_FILE -DSEAM_TEE`,
which writes `OUTPUT.BIN` *and* mirrors every message to COM1), runs it under
DOSBox-X on a private Xvfb display with COM1 bridged to TCP, connects the C#
client with `--dump`, and verifies with `tools/stream/compare_streams.py` that
every received message is byte-identical to the seam's own `OUTPUT.BIN` from
the same session. Messages sent before the client connected are expected and
reported, not failures: the client asks for a keyframe and resynchronises.

The comparison is by sequence number. The file backend batches its writes, so
the file copy lags the wire by the frame that is still in flight; those
messages are reported as in flight, not as a mismatch. A gap in the received
stream (a message that never arrived) and a message that is in both streams but
differs are failures.

The emulated serial link is lossy in its own right: when the host's TCP thread
stalls, the emulator's nullmodem buffer overflows and a chunk of the stream is
lost, which shows up as such a gap. The check therefore runs the session twice
when the first one shows a loss, and fails only when both sessions lose data.

    sh tests/link/run.sh [--seconds 40] [--display :88] [--port 2324] [--no-build]

Needs `uasm`, `dosbox-x`, `Xvfb` and the .NET SDK.

## Beyond these

- `sh engine/asm/swreg/tools/verify.sh` - assembler gate: the default build must
  stay byte-identical to the reference (`SWREG.EXE: OK`, `IDENTICAL`) and the
  non-seam source must match `swreg.pin`.
- `sh build.sh --variant file` + `sh tools/dosbox/stream.sh 45` - the file
  backend plus Python replay (see `docs/stream.md`).
- `tools/repro/nullmodem-0xff/` - why the wire doubles `0xFF` and which switch
  turns it off.
