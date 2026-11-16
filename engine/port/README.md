# engine/port

The B2 real C port. This is the C deliverable, not the B1 executable spec in
`engine/c/`:

- ordinary C11 functions, structs, enums and loops;
- no assembly and no generated instruction stream;
- no guest CPU, registers, flags, flat `mem[]` address space or
  segment:offset dispatcher;
- no DOS/BIOS/EMS emulation in the game logic;
- platform access through small, ordinary C interfaces.

B1 stays buildable as the differential oracle. The two trees never link each
other.

## First slice: live party state and the save container

The first vertical slice is the state core and the save loader:

| Path | What |
|---|---|
| `include/yt2/party.h` | live party/character/stat/item model (`yt2_party`, `yt2_character`, `yt2_stats`, enums) |
| `include/yt2/savefile.h` | `CURGAME`/`SAVEGAMEn` chunk table, `yt2_save` container |
| `include/yt2/world.h` | `WORLD.DAT` map prefix: tiles, map descriptors, map names |
| `include/yt2/pictures.h` | `PICTURES.VGA` group table and picture accessors |
| `include/yt2/palette.h` | 256-entry 6-bit VGA palette + the D0h..DFh rotation |
| `include/yt2/pcx.h` | single-plane 8-bit PCX decode (the VBE logo format) |
| `include/yt2/surface.h` | 8-bit indexed surface, clipped blits, masked blits |
| `include/yt2/text.h` | the executable's standard 6x6 text face |
| `include/yt2/cluebook.h` | the clue-book/help page composer |
| `include/yt2/catalog.h` | the clue-book map catalog (F1) |
| `include/yt2/lists.h` | the shared list-page composer (F1, F2, ...) |
| `include/yt2/mapview.h` | the overhead map composer (F1, Enter) |
| `include/yt2/monster_detail.h` | the monster detail view (F2 Enter) |
| `include/yt2/monsters.h` | the monster statistics list (F2) |
| `include/yt2/spells.h` | the spell information list (F3) |
| `include/yt2/spell_detail.h` | the spell detail page (F3 Enter) |
| `include/yt2/item_detail.h` | the item page (F5 Enter) |
| `include/yt2/classes.h` | the class list and spell details (F4) |
| `include/yt2/items.h` | the categories, item details, info screens (F5) |
| `include/yt2/walkthrough.h` | the 31-page walkthrough (F6) |
| `include/yt2/cursor.h` | the software mouse cursor overlay |
| `include/yt2/protocol.h` | companion protocol v0 framing + VIDEO/INPUT/CONTROL/AUDIO |
| `include/yt2/transport.h` | the YT2B folder link (OUTPUT.BIN/INPUT.BIN) |
| `include/yt2/present.h` | the engine side of the VIDEO boundary |
| `include/yt2/clock.h` | host monotonic clock for transport stamps |
| `include/yt2/boot.h` | the static start-up scenes and their palettes |
| `include/yt2/file.h` | the small platform file service |
| `include/yt2/endian.h` | explicit little-endian reads/writes |
| `include/yt2/intro.h` | the boot/menu state machine + attract cutscene (`yt2_intro`) |
| `include/yt2/create.h` | character creation + roster sessions (`yt2_create`, `yt2_roster`) |
| `src/party.c` | parses the 5000-byte party block into live state and writes it back |
| `src/savefile.c` | loads/writes the 77509-byte save, validates the chunks, owns the bytes |
| `src/world.c` | parses the map grid, descriptors, names and nine palettes |
| `src/pictures.c` | indexes the ten picture groups and their packed images |
| `src/surface.c` | clears and blits into the indexed surface |
| `src/text.c` | draws transparent/opaque 6x6 glyphs |
| `src/cluebook.c` | composes picture, text and the command icon bar |
| `src/cursor.c` | draws the masked 16x16 cursor at a position |
| `src/palette.c` | writes one rotation phase over D0h..DFh |
| `src/pcx.c` | decodes the WORLD.DAT LOGO.PCX embed to 640x480 indices |
| `src/protocol.c` | v0 frame/message encode (codec-1 runs, AUDIO) + input decode |
| `src/transport_folder.c` | timed YT2B writer, input follower |
| `src/present.c` | presents with palette-span and row diffing |
| `src/clock.c` | PIT-unit stamps, millisecond sleep |
| `src/catalog.c` | the 55-entry map table plus the F1 page over the composer |
| `src/lists.c` | the shared composer: frame, header, rows, MORE, icons |
| `src/mapview.c` | composes the tile/icon overhead map with legend markers |
| `src/monster_detail.c` | the F2 detail composer plus the portrait tables |
| `src/monsters.c` | the 61-record table plus the F2 page over the composer |
| `src/spells.c` | the 105-record table plus the F3 page over the composer |
| `src/spell_detail.c` | the F3 detail composer plus the frozen page tables |
| `src/item_detail.c` | the F5 page composer (reads disk structs) |
| `src/classes.c` | the class lines plus F4 list/detail over the composer |
| `src/items.c` | the item table plus F5 screens over/around the composer |
| `src/walkthrough.c` | the text pages plus the F6 screen |
| `src/boot.c` | composes the webfoot, story-text, attract, title and menu scenes |
| `src/intro.c` | the boot/menu flow and the animated attract loop over `yt2_intro` |
| `src/create.c` | creation/roster engine: stages, RNG, roll/derive/equip, composers |
| `src/tool_info.c` | `yt2p-info`, the differential save reader |
| `src/tool_world.c` | `yt2p-world`, the map-database reader |
| `src/tool_pictures.c` | `yt2p-pictures`, the picture-group reader |
| `src/tool_render.c` | `yt2p-render`, a picture/palette PPM renderer |
| `src/tool_cluebook.c` | `yt2p-cluebook`, the clue-book page renderer |
| `src/tool_catalog.c` | `yt2p-catalog`, the map catalog renderer |
| `src/tool_mapview.c` | `yt2p-mapview`, the overhead map renderer |
| `src/tool_monsters.c` | `yt2p-monsters`, the monster list/detail renderer |
| `src/tool_spells.c` | `yt2p-spells`, the spell list/detail renderer |
| `src/tool_classes.c` | `yt2p-classes`, the class list/detail renderer |
| `src/tool_items.c` | `yt2p-items`, the F5 screen renderer |
| `src/tool_walkthrough.c` | `yt2p-walkthrough`, the F6 page renderer |
| `src/tool_play.c` | `yt2p-play`, menu loop (creation/roster live) + scene demo |
| `src/tool_boot.c` | `yt2p-boot`, the start-up scene renderer |
| `src/tool_intro.c` | `yt2p-intro`, the attract-loop frame/audio renderer |
| `tests/` | unit tests and the gate |

`yt2_world` parses the map prefix into live tiles and descriptors, the nine
6-bit palettes and keeps the rest of the file for the music/SFX loaders.
`yt2_pictures` indexes the ten raw 8-bit picture groups and hands out a
pointer per picture.  `yt2_surface` is the ordinary indexed framebuffer.
`yt2_save` keeps the file image because the object/level/world blocks are not
decoded yet; the party
block is parsed into `yt2_party` and is the first live game state.
`yt2_save_write` copies the image, writes the modeled party fields back over
the party block, and leaves every unmodeled byte (reserved spans, containers,
the other six blocks) untouched, so an untouched load/write is byte-identical. Unknown party header/slot fields are not carried into the live
model yet; they are preserved, not interpreted.

## Build and test

    sh engine/port/tools/build.sh
    build/port/yt2p-tests
    sh engine/port/tests/run.sh
    sh tests/port-live/run.sh

The default build ships the player and the unit tests only; the
per-slice `yt2p-*` dev tools below build on request:

    sh engine/port/tools/build.sh tools

The intro gate renders two attract cycles and checks them behaviorally
against the frozen B1 log (frames on stdout, audio triggers on stderr):

    build/port/yt2p-intro PICTURES.VGA WORLD.DAT --frames 14000 \
      >build/tmp/port.intro.log 2>build/tmp/port.intro.audio
    python3 engine/port/tests/check_intro.py \
      engine/port/tests/intro_ref.log build/tmp/port.intro.log \
      --artifacts engine/port/tests/intro_artifacts.txt \
      --audio engine/port/tests/intro_audio.txt build/tmp/port.intro.audio \
      --rows engine/port/tests/intro_rows_c1.txt

## Watch it live

`yt2p-play` is the engine side of the v0 boundary: it composes screens,
presents them over the folder link, reads input back and steps the palette
rotation on the engine tick.  Run it with the client in one command:

    sh tools/play-port.sh [--spelltext | --spelltext-complete]

`--spelltext` reads the F3 detail EFFECT lines live from WORLD.DAT, so a
spelltext-patched world (`tools/yt2_spelltext.py`) shows its descriptions:
the official 92-spell table, or all 105 with `--spelltext-complete` (13
curated lines).  A vanilla world renders pixel-identical either way.

(For a redistributable bundle instead: `sh tools/publish-linux.sh --server
port --build`, then `build/dist/yt2-linux-x64/play.sh`.)

Keys 1-6 switch scenes (help, catalog, overhead map, title, menu,
monsters), and the clue-book keys work as in the game: F1 opens the catalog,
F2 the monsters, F3 the spells, F4 the classes, F5 the categories, F6 the
walkthrough, Up/Down move the gold selection, PgDn/PgUp turn walkthrough
pages, Enter opens the selected map, a monster's statistics, a line's
spells or a category's items, ESC goes back.  Clicking a list row selects it
and double-clicking opens it; the MORE arrows page the window and a marker
click on the overhead map shows its legend.  The scroll icon
opens the spells, the book the classes, the chest the items, the quill
the walkthrough, the detail footer returns to the list, the EXIT icon
quits, the mouse moves the hand cursor, Q quits.  `server-start.sh` and
`client-start.sh` run either side alone.  The player emits one TICK pace
record per session (the frame budget, 55 ms) and a tick counter after
every frame; the client applies them and echoes `ticks=`/`paces=`, and
the stream tools (`check_stream.py`, `replay.py`) check tick
adjacency/monotonicity and replay at the recorded pace.

The gate builds with `-Wall -Wextra -Wpedantic -Werror`, runs the unit tests,
and on a real save compares `yt2p-info` output byte-for-byte with B1's
`build/c/yt2saveinfo`. It also writes the loaded save back and requires the
round trip to be byte-identical, then moves the party and requires B1 to
read the new position while every other byte stays put.  It also loads the
shipped `WORLD.DAT` and `PICTURES.VGA` (from `build/swreg/oracle/` or
`_ref/orig-bin/YT2/`), checks the tile/descriptor counts and pins known tiles,
maps, names, the nine palette checksums and a checksum for every picture
group.  It then renders the title picture (group 0, picture 22) at (1,0) with
palette 0 and requires the original's exact frame hash
`09BEC5E9162924EF` and palette hash `E3275EE3AE459FB5`, and the main-menu
picture (group 0, picture 2) at (1,1) with frame hash
`6ADE1397DAE95BC0`.
The clue-book page (F8 in play, Tab from the menu) is group 0 picture 13 at
(1,1) plus transparent text and the command bar; it renders to cursor-free
frame `07D368FE31D7447F` with palette 0
(`E3275EE3AE459FB5`).
The F1 map catalog (group 0 picture 14, the 55-entry list, MORE arrows, icon
bar) renders map 67 to cursor-free frame `064C1427810F47E3`, and the overhead
map of map 67 (tile/object icons plus legend markers) to frame
`725194639EB33B4F` with palette 0.
The F2 monster list renders rank 0 to `D5B4C34273DCD369` and rank 60 to
`6E3F491525EA591E`, cursor-free; both reproduce their B1 framebuffers with the
hand cursor drawn at (24,170).
The F3 spell list renders ranks 0/14/98/104 to
`336AED2DF4355F25`/`4E0021E2C76A7281`/`14A80E5E78EF674E`/`EB4FF720BE73034E`,
cursor-free (the original doubles BLOCK OF POWER at ranks 5 and 11 and omits
BEAM OF DEATH); plus the hand at (24,170) they reproduce the B1 framebuffers
`CFDBBECC9F9F16B5`, `2429157AFD25AAC1`, `EE0BBBD290210786`, `C4B3A494D5A4A386`.
The F4 class list renders ranks 0/5 to `635F5999F0C8700A`/`3E56F34BA5FD770A`
(no MORE line on one page), cursor-free, reproducing `7AFA214601883F62`
and `55F1BAF7B6BD4662` with the hand; each of the six spell-by-class
details pins its first/last ranks the same way (monk `76A5453A840960FC` /
`42B33D023F962D96`, marksman `A85D9A37F1537BA3` / `9FB6858F0C9BEF02`, and
four more in the gate).
The F5 category list renders ranks 0/7 to `52651339B2A27723` /
`4A2B69FF4D3178A3`; the six item details pin first/last ranks (weapons
`79D758C1D972A566` / `6CCE5BE1E98A356E with the hand) and the two info
screens pin whole frames, all reproducing their B1 framebuffers with the
hand at (24,170).
The F6 walkthrough renders pages 0/1/14/19/30 to `2006DFD600464B52` /
`22F421C5C49220A1` / `A2B9C0771EAF4A5D` / `964B04246F019CFC` /
`8A6765000C222B95`, cursor-free, reproducing `687B3AA93D1BEBEC`,
`82CDA8EC351D07E3`, `80161B205CE19807`, `21724493C02D833A`,
`5605EEBBEFD64001` with the hand.
The cursor unit draws the hand (group 8 picture `1Dh`) onto a composition:
catalog plus cursor reproduces the oracle framebuffer `BC1E5BFF608CB41B`,
help page plus cursor `7F29E8DA4ED08B2F`.  The rotation unit turns the 48
animation-base bytes from `WORLD.DAT` through the four live D0h..DFh phases,
all pinned byte-exact against the B1 stream.
The ten static start-up scenes must match the original's checkpoint pairs
exactly: webfoot `C5B1519EF4C1133A`/`089875EBAB4D4C6F`, story sleep
`FB816B027A864AD1` and story knock `033C9733AF00EB97` with
`16E60732045316F6`, the five attract pictures with
`E3275EE3AE459FB5`, and the title/menu frames.
`YT2_SAVE=path`, `YT2_WORLD=path` and `YT2_PICTURES=path` make the unit tests
check specific files.

`yt2p-info SAVEGAME` prints the seven chunks, the party position and every
non-empty roster slot through the B2 live model. It is intentionally the same
text as B1's reader for the differential gate.  `yt2p-world WORLD.DAT` prints
the map counts and every non-filler map with its name; `yt2p-pictures
PICTURES.VGA` prints every group's bounds and checksum; `yt2p-render
PICTURES.VGA WORLD.DAT GROUP INDEX PALETTE OUT.ppm [X Y]` blits one picture
onto a black 320x200 surface and writes a PPM with the 6-bit palette expanded.

## Next slices

1. the animated story cutscene and the boot/menu state machine (done,
   gated by `check_intro.py` against B1 over two attract cycles);
2. the clue-book list pages (F3-F6; F1 maps and F2 monsters done);
3. boundary completion: row diffing (done), codec 1 (done, renders in
   the C# client), AUDIO encoders (done; no live source yet - M9), scroll
   (done), TICK (done: monotonic ticks + pace, gated);
3. exploration, objects and scripted events;
4. dialogue, combat, training/inventory/shops;
5. the remaining data loaders and the companion host boundary.

Every slice is gated against B1 and the DOS oracle; see `docs/porting.md` §3.2.
