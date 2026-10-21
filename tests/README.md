# Tests

Four checks pin the client end of the serial seam link. All are ordinary shell
scripts; artifacts land under `build/runs/<kind>/<stamp>/`.

## `tests/replay/run.sh`

Offline and hermetic (no DOSBox-X, no game): replays `tests/data/sample-video.bin`
through the C# client and through the Python reference decoder
(`tools/stream/replay.py`, `--ppm` in both) and compares the final frames with
`cmp`. Same framing, same palette expansion, same state, byte for byte.

    sh tests/replay/run.sh [RECORDING]

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

Live: builds the tee variant of the engine (`-DSEAM -DSEAM_FILE -DSEAM_TEE`,
which writes `STREAM.BIN` *and* mirrors every message to COM1), runs it under
DOSBox-X on a private Xvfb display with COM1 bridged to TCP, connects the C#
client with `--dump`, and verifies with `tools/stream/compare_streams.py` that
every received message is byte-identical to the seam's own `STREAM.BIN` from
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
