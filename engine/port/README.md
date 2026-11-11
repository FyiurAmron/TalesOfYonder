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
| `include/yt2/palette.h` | 256-entry 6-bit VGA palette |
| `include/yt2/surface.h` | 8-bit indexed surface and clipped blits |
| `include/yt2/file.h` | the small platform file service |
| `include/yt2/endian.h` | explicit little-endian reads/writes |
| `src/party.c` | parses the 5000-byte party block into live state and writes it back |
| `src/savefile.c` | loads/writes the 77509-byte save, validates the chunks, owns the bytes |
| `src/world.c` | parses the map grid, descriptors, names and nine palettes |
| `src/pictures.c` | indexes the ten picture groups and their packed images |
| `src/surface.c` | clears and blits into the indexed surface |
| `src/tool_info.c` | `yt2p-info`, the differential save reader |
| `src/tool_world.c` | `yt2p-world`, the map-database reader |
| `src/tool_pictures.c` | `yt2p-pictures`, the picture-group reader |
| `src/tool_render.c` | `yt2p-render`, a picture/palette PPM renderer |
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

The gate builds with `-Wall -Wextra -Wpedantic -Werror`, runs the unit tests,
and on a real save compares `yt2p-info` output byte-for-byte with B1's
`build/c/yt2saveinfo`. It also writes the loaded save back and requires the
round trip to be byte-identical, then moves the party and requires B1 to
read the new position while every other byte stays put.  It also loads the
shipped `WORLD.DAT` and `PICTURES.VGA` (from `build/swreg/oracle/` or
`_ref/orig-bin/`), checks the tile/descriptor counts and pins known tiles,
maps, names, the nine palette checksums and a checksum for every picture
group.  It then renders the title picture (group 0, picture 22) at (1,0) with
palette 0 and requires the original's exact frame hash
`09BEC5E9162924EF` and palette hash `E3275EE3AE459FB5`, and the main-menu
picture (group 0, picture 2) at (1,1) with frame hash
`6ADE1397DAE95BC0`.
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

1. boot/menus and the clue-book/help shell;
2. exploration, objects and scripted events;
3. dialogue, combat, training/inventory/shops;
4. the remaining data loaders and the companion host boundary.

Every slice is gated against B1 and the DOS oracle; see `docs/porting.md` §3.2.
