YT2 client + C server bundle - Windows
======================================

What it is
----------
A development client for the transliterated C engine (`yt2game.exe`), bundled
with the engine itself.  The game runs natively - no DOSBox-X - and talks to
the client through the same folder link as the asm seam build:
`server\game\OUTPUT.BIN` carries the screen, palette and audio messages,
`server\game\INPUT.BIN` carries keyboard and mouse input.  Both files are
timed `YT2B` pseudo-rings; the client follows the output file and appends
input as it happens.

The engine is the same one that passes the M3 intro/attract, M5 gameplay,
save/load and sound gates, cross-built for Windows x64 with MinGW-w64.  It
presents the VBE intro modes and mode 13h, emulates the resident SBFMDRV for
FM music and PC-speaker audio, and streams the game's WORLD.DAT effect
records.  The client synthesises the audio and writes nothing to disk beyond
its own logs.

Contents
--------
  client\Yt2Client.exe     self-contained (no .NET install needed), 64-bit
  client\SDL3.dll          SDL3 win32-x64 runtime (official libsdl-org release)
  client-start.bat         follows the live timed BIN transport
  server-start.bat         starts yt2game.exe with the folder link
  play.bat                 starts the server, then the client
  smoke-test.bat           replays sample\OUTPUT.BIN, no engine needed
  sample\OUTPUT.BIN        a timed engine-to-client stream
  server\                  the engine side:
    game\yt2game.exe         the transliterated C engine (PE32+ x86-64)
    game\                    the original game data
    game\OUTPUT.BIN          the screen/palette/audio link, created on first run
    game\INPUT.BIN           the input link, created on first run
    capture\                 optional captures

How to run
----------
  play.bat                   one command: engine + client window
or, in two windows:
  server-start.bat           the engine starts the intro
  client-start.bat           the client window mirrors the display

The client asks the engine for a keyframe when it connects, so the start order
does not matter and a client that starts late resynchronises itself.

Play in the *client* window: keystrokes and the mouse are sent to the game.
The pointer stays visible and is not captured: over the client window it moves
the game's cursor, and leaving the window hands it back to the desktop.  Close
the window or press Ctrl+Q to quit the client (Esc is forwarded to the game).
`play.bat` stops the engine when the client exits; `server-start.bat` on its
own keeps running until interrupted.

Replay and diagnostics
----------------------
Replay the last run at its captured pace:

  client\Yt2Client.exe --file server\game\OUTPUT.BIN

The engine itself also has a headless mode and is used by the gates:

  server\game\yt2game.exe --game server\game --ticks 3600 --present-log present.txt
  server\game\yt2game.exe --game server\game --link --trace-input

`--live` is the server launcher's mode (link plus real-time pacing at the guest
clock, 3,000 cycles/ms by default); `--link` runs the link as fast as the host
can, which is what the transport tests use.  The typed save reader
(`yt2saveinfo.exe`) is built next to the engine but not shipped in this bundle.
