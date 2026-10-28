YT2 client + asm server bundle - Linux
======================================

What it is
----------
A development client for the instrumented SWREG.EXE ("seam" build), bundled
with the asm server: the game
streams its screen (the 640x480 VBE intro and the mode-13h game) and palette
into OUTPUT.BIN in its working directory, and the client sends keyboard and
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
  client-start.sh          follows the live timed BIN transport
  server-start.sh          starts DOSBox-X with the folder-link profile
  play.sh                  starts the server, then the client
  smoke-test.sh            replays sample/OUTPUT.BIN, no DOSBox-X needed
  sample/OUTPUT.BIN        a timed engine-to-client stream
  server/                  the game side:
    dosbox-x.conf            no serial port; the link is OUTPUT.BIN/INPUT.BIN
    game/                    the seam build + game files + the link files
    capture/                 DOSBox-X screenshots and optional YT2S captures

How to run
----------
  ./play.sh                 one command: server window + client window
or, in two terminals:
  ./server-start.sh         the game window opens, the intro starts
  ./client-start.sh         the client window mirrors the display

The client asks the engine for a keyframe when it connects, so the start
order does not matter; if it connects late it resynchronises by itself. Both
`OUTPUT.BIN` and `INPUT.BIN` carry timestamps around their unchanged v0 frames.
The client follows the server's output file and appends timed input records,
so each transport file is directly usable for replay.

Replay the last run at its captured pace with:

  ./client/Yt2Client --file server/game/OUTPUT.BIN

Replay saved input into a fresh server run with:

  cp server/game/INPUT.BIN server/capture/INPUT.BIN
  ./client/Yt2Client --follow server/game/OUTPUT.BIN \
      --input server/game/INPUT.BIN --replay-input server/capture/INPUT.BIN

An additional full-duplex YT2S capture is optional (`--record-session PATH`).

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
Replays the bundled timed BIN stream in a window, prints a
"client: done  frames=... rects=... palette=... records=..." line and writes
sample/smoke.ppm.  Exit code 0 means the client is fine.
