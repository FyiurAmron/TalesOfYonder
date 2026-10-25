YT2 companion client - Windows build
====================================

What it is
----------
A development client for the instrumented SWREG.EXE ("seam" build): the game
streams its screen (the 640x480 VBE intro and the mode-13h game) and palette
into STREAM.BIN in its working directory, and the client sends keyboard and
mouse input back through INPUT.BIN next to it.  Both files are pseudo-rings:
at a size limit the writer truncates them to zero and restarts from offset 0,
and the other side notices the shrink and rewinds.  This bundle runs the
client natively on Windows.

The sound follows the same link: the game streams the resident driver's OPL
register writes, the PC-speaker gate and its sound-effect calls; the client
synthesises the FM music and the beep, and plays the digitized effect records
from `server\game\WORLD.DAT`.  The bundled DOSBox-X profile sets
`nosound = true`, so the emulated Sound Blaster/OPL
still runs (the game needs it) but DOSBox-X does not play a second, delayed
copy; the client is the only audio output.  To hear the emulator instead, set
`nosound = false` in `server\dosbox-x.conf` and run `client-start.bat
--no-audio`.

Contents
--------
  client\Yt2Client.exe     self-contained (no .NET install needed), 64-bit
  client\SDL3.dll          SDL3 win32-x64 runtime (official libsdl-org release)
  client-start.bat         follows server\game\STREAM.BIN
  server-start.bat         starts DOSBox-X with the folder-link profile
  play.bat                 starts the server, then the client
  smoke-test.bat           replays sample\stream.bin, no DOSBox-X needed
  sample\stream.bin        a short no-input recording
  server\                  the game side:
    dosbox-x.conf            no serial port; the link is STREAM.BIN/INPUT.BIN
    game\                    the seam build + game files + the link files
    capture\                 DOSBox-X screenshots land here

How to run
----------
  play.bat                  one command: server window + client window
or, in two windows:
  server-start.bat          the game window opens, the intro starts
  client-start.bat          the client window mirrors the display

The client asks the engine for a keyframe when it connects, so the start
order does not matter; if it connects late it resynchronises by itself.  The
client opens STREAM.BIN read-only with write sharing and appends to INPUT.BIN,
so both programs can run at the same time.

Play in the *client* window: keystrokes and the mouse are sent to the game.
The pointer stays visible and is not captured: over the client window it
moves the game's cursor, and leaving the window hands it back to the desktop.
Close the window or press Ctrl+Q to quit the client (Esc is forwarded to the
game).  The DOSBox-X window can stay behind it.

If DOSBox-X asks for a working directory ("select a folder"), you are running
it without the bundle's launcher: server-start.bat passes -nopromptfolder and
the bundled profile sets "working directory option = noprompt".  Running
"dosbox-x -conf dosbox-x.conf" by hand from another directory hits the dialog
because DOSBox-X cannot resolve the relative config path.

Quick check without DOSBox-X
----------------------------
Run smoke-test.bat.  It replays the bundled recording *in a window* (so it also
checks client\SDL3.dll and the renderer), prints a
"client: done  frames=… rects=… palette=… records=… bytes=…" line and writes
sample\smoke.ppm (320x200 PPM of the last frame).  Close the window when the
replay ends.  Exit code 0 means the client binary, the window, the decoder and
the protocol are fine.

The first line the client prints is the window it created:

    client: window 960x600 (windows / direct3d11)

If that line is missing, no window was created and the console says why.

Client options (see --help)
---------------------------
  --tcp [host:]port   connect (default 127.0.0.1:2324)
  --file PATH         replay a recording instead of connecting
  --scale N           window scale (default 3 -> 960x600)
  --stats SECONDS     progress line interval, 0 disables
  --headless          no window, decode only
  --no-audio          do not open an audio device
  --dro PATH          render a DOSBox-X raw OPL capture (offline test)
  --wav PATH          where --dro writes its WAV
  --ppm PATH          write the last frame as a PPM
  --dump PATH         record every byte the decoder saw
  --raw               do not unescape the nullmodem 0xFF doubling
  --no-keyframe       do not ask for a keyframe on connect
  --key S:SCAN:ASCII  press a key S seconds after starting (repeatable)
  --mouse S:DX:DY     move the mouse DX,DY pixels at S seconds (repeatable)
  --click S:BTN:0|1   button edge at S seconds (0 left, 2 right; repeatable)

Troubleshooting
---------------
* Console only, no window: read the lines after "client:".  "SDL_Init failed",
  "SDL_CreateWindow failed" or a DllNotFound message all mean SDL could not
  start; the commonest cause is client\SDL3.dll missing (it ships there - do
  not move the exe out of the bundle).
* "follow: waiting for ...STREAM.BIN" means the engine has not created the
  file yet.  Start server-start.bat; the client may also start first and waits
  for the file.  If it never appears, the game side is not running (see the
  next point).
* No DOSBox-X window at all: the bundle does not include dosbox-x.exe; put
  it in server\ (or on PATH) - server-start.bat prints a message and pauses
  when it cannot find it.  When the launcher does reach DOSBox-X it writes
  server\start-server.log with the resolved path and, later, the exit code.
* The window appears but stays black: that is the game still fading in, or the
  engine has not reached its seam yet (about ten seconds of boot).  The client
  retries its handshake every second and asks for a keyframe.
* The client window opens behind the DOSBox-X window: click it in the taskbar.
  The DOSBox-X window can be minimised once the client shows the game.
* The pointer is never captured: over the client window it moves the game's
  cursor, and leaving the window hands it back to the desktop.  Press Ctrl+Q
  (or close the window) to quit the client; Esc is forwarded to the game.
* No sound: the client prints `client: audio 44100 Hz stereo s16` when the
  device opened.  If that line is missing, the console says why (no device,
  or `--no-audio`); the picture keeps running.  A `client: samples
  unavailable` line means `server\game\WORLD.DAT` is missing, so the
  digitized effects stay silent (music and the beep still play).  DOSBox-X
  is silent by design (see "What it is").

Notes
-----
* Both engine modes are streamed: the 640x480x8 VBE intro (the client window
  resizes itself) and the 320x200 mode-13h game.
* Mouse input goes through an INT 33h shim in the engine, so the game's own
  software cursor follows the client.
* The folder link is the bundle's transport: the engine appends to
  STREAM.BIN and reads INPUT.BIN, both pseudo-rings (8 MiB video, 64 KiB
  input) that wrap and resynchronise.  The client still speaks the old
  TCP/nullmodem link with --tcp when it is built against the serial engine.
* Provenance: SDL3.dll comes from the pinned SDL3 win32-x64 release listed in
  client/publish-win.sh (the script verifies its sha256); Yt2Client.exe is
  built from client/ with that script.
