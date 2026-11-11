YT2 client + B2 port server bundle - Linux
=========================================

What it is
----------
A development client for the B2 real C port (`yt2p-play`), bundled with the
player itself.  The engine runs natively - no DOSBox-X, no emulation - and
talks to the client through the same folder link as the other engines:
`server/game/OUTPUT.BIN` carries the screen and palette, `server/game/INPUT.BIN`
carries keyboard and mouse input.  Both files are timed `YT2B` pseudo-rings;
the client follows the output file and appends input as it happens.

This is the boundary demo, not the game: thirteen composed scenes (help
page, map catalog, overhead map, title, menu, monster list, spell list,
class list, class spells, categories, category items, walkthrough,
monster statistics), a hand cursor tracked from the mouse, and the
palette rotation stepped on the engine tick.  Every pixel it shows is
differentially gated against the B1 executable spec and the DOS oracle.

Contents
--------
  client/Yt2Client        self-contained (no .NET install needed), 64-bit
  client/libSDL3.so.0     SDL3 runtime, loaded by name
  client-start.sh         follows the live timed BIN transport
  server-start.sh         starts the B2 player with the folder link
  play.sh                 starts the server, then the client
  smoke-test.sh           replays sample/OUTPUT.BIN, no engine needed
  sample/OUTPUT.BIN       a timed engine-to-client stream
  server/game/            the engine and game files:
    yt2p-play               the B2 boundary player
    WORLD.DAT, PICTURES.VGA the original game data it reads
    OUTPUT.BIN, INPUT.BIN   the link files, created on the first run

How to run
----------
  play.sh                   one command: engine + client window
or, in two windows:
  server-start.sh           the player presents the catalog
  client-start.sh           the client window mirrors the display

The client asks the engine for a keyframe when it connects, so the start order
does not matter and a client that starts late resynchronises itself.

Play in the *client* window: keys 1-6 switch scenes, and the clue-book keys
work as in the game (F1 opens the catalog, F2 the monsters, F3 the spells,
F4 the classes, F5 the categories, F6 the walkthrough, Up/Down move the
selection, PgDn/PgUp turn walkthrough pages, Enter opens the map, a
monster's statistics, a line's spells or a category's items, ESC goes
back).  Clicking a list row
selects
it and double-clicking opens it; the MORE arrows page the window, a
marker click on the overhead map shows its legend, and the EXIT icon
quits.  The mouse moves the hand cursor and Q quits the
player.  The pointer stays visible and is not captured: over the client
window it moves
the game's cursor, and leaving the window hands it back to the desktop.  Close
the window or press Ctrl+Q to quit the client.  `play.sh` stops the engine
when the client exits; `server-start.sh` on its own keeps running until
interrupted.

Replay and diagnostics
----------------------
Replay the last run at its captured pace:

  client/Yt2Client --file server/game/OUTPUT.BIN
