YT2 companion client - Linux build
==================================

What it is
----------
A development client for the instrumented SWREG.EXE ("seam" build): the game
streams its screen (the 640x480 VBE intro and the mode-13h game) and palette
into STREAM.BIN in its working directory, and the client sends keyboard and
mouse input back through INPUT.BIN next to it.  Both files are pseudo-rings:
at a size limit the writer truncates them to zero and restarts from offset 0,
and the other side notices the shrink and rewinds.  This bundle runs the
client natively on Linux.

The sound follows the same link: the game streams the resident driver's OPL
register writes, the PC-speaker gate and its sound-effect calls; the client
synthesises the FM music and the beep, and plays the digitized effect records
from `server/game/WORLD.DAT`.  The bundled DOSBox-X profile sets
`nosound = true`, so the emulated Sound Blaster/OPL
still runs (the game needs it) but DOSBox-X does not play a second, delayed
copy; the client is the only audio output.  To hear the emulator instead, set
`nosound = false` in `server/dosbox-x.conf` and run `./client-start.sh
--no-audio`.

Contents
--------
  client/Yt2Client         self-contained 64-bit client (no .NET needed)
  client/libSDL3.so.0      SDL3 runtime, used by the client
  client-start.sh          follows server/game/STREAM.BIN
  server-start.sh          starts DOSBox-X with the folder-link profile
  play.sh                  starts the server, then the client
  smoke-test.sh            replays sample/stream.bin, no DOSBox-X needed
  sample/stream.bin        a short no-input recording
  server/                  the game side:
    dosbox-x.conf            no serial port; the link is STREAM.BIN/INPUT.BIN
    game/                    the seam build + game files + the link files
    capture/                 DOSBox-X screenshots land here

How to run
----------
  ./play.sh                 one command: server window + client window
or, in two terminals:
  ./server-start.sh         the game window opens, the intro starts
  ./client-start.sh         the client window mirrors the display

The client asks the engine for a keyframe when it connects, so the start
order does not matter; if it connects late it resynchronises by itself.  The
client opens STREAM.BIN read-only with write sharing and appends to INPUT.BIN,
so both programs can run at the same time.

Play in the *client* window: keystrokes and the mouse are sent to the game.
The pointer stays visible and is not captured.  Its position in the client
window maps proportionally to the same place in the game (upper-left to
upper-left, centre to centre), for both the 320x200 game and the 640x480
intro.  Close the window or press Ctrl+Q to quit the client (Esc is forwarded
to the game).

Requirements
------------
  dosbox-x on PATH (https://dosbox-x.com/) for the server side.
  The client is self-contained; SDL3 sits next to it.

Smoke test without DOSBox-X
---------------------------
  ./smoke-test.sh
Replays the bundled recording in a window, prints a
"client: done  frames=... rects=... palette=... records=..." line and writes
sample/smoke.ppm.  Exit code 0 means the client is fine.
