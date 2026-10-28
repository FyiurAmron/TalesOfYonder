YT2 client + C server bundle - Linux
====================================

What it is
----------
A development client for the transliterated C engine (`yt2game`), bundled
with the engine itself.  The game runs natively - no DOSBox-X - and talks to
the client through the same folder link as the asm seam build:
`server/game/OUTPUT.BIN` carries the screen and palette, `server/game/INPUT.BIN`
carries keyboard and mouse input.  Both files are timed `YT2B` pseudo-rings;
the client follows the output file and appends input as it happens.

This is the same engine that passes the M3 intro/attract checkpoint gate, now
wired to the live display link.  Presentation is mode 13h (320x200x8); the VBE
intro modes and the FM/PC-speaker audio stream arrive with M4/M6, so the C
server is currently silent and skips VBE-only pictures.

Contents
--------
  client/Yt2Client        self-contained (no .NET install needed), 64-bit
  client/libSDL3.so.0     SDL3 runtime, loaded by name
  client-start.sh         follows the live timed BIN transport
  server-start.sh         starts the C engine with the folder link
  play.sh                 starts the server, then the client
  smoke-test.sh           replays sample/OUTPUT.BIN, no engine needed
  sample/OUTPUT.BIN       a timed engine-to-client stream
  server/game/            the engine and game files:
    yt2game                 the transliterated C engine
    CURGAME, WORLD.DAT, PICTURES.VGA   the original game data
    OUTPUT.BIN, INPUT.BIN   the link files, created on the first run

How to run
----------
  play.sh                   one command: engine + client window
or, in two windows:
  server-start.sh           the engine starts the intro
  client-start.sh           the client window mirrors the display

The client asks the engine for a keyframe when it connects, so the start order
does not matter and a client that starts late resynchronises itself.

Play in the *client* window: keystrokes and the mouse are sent to the game.
The pointer stays visible and is not captured: over the client window it moves
the game's cursor, and leaving the window hands it back to the desktop.  Close
the window or press Ctrl+Q to quit the client (Esc is forwarded to the game).
`play.sh` stops the engine when the client exits; `server-start.sh` on its own
keeps running until interrupted.

Replay and diagnostics
----------------------
Replay the last run at its captured pace:

  client/Yt2Client --file server/game/OUTPUT.BIN

The engine itself also has a headless mode and is used by the gates:

  server/game/yt2game --game server/game --ticks 2500 --present-log present.txt
  server/game/yt2game --game server/game --link --trace-input

`--live` is the server launcher's mode (link plus real-time pacing at the
oracle's 30,000 cycles/ms); `--link` runs the link as fast as the host can,
which is what the transport tests use.
