# B1 data structures (executable spec)

`engine/c/include/yt2/data.h` is the typed description of the original's
**on-disk** records: the save file (`CURGAME` / `SAVEGAMEn`), the party header
and roster slots, the DOS file struct, the picture-cache class table, the
`WORLD.DAT` music header/patch and the SFX table. This is the B1 pseudo-port's
record model, useful documentation of the formats for the B2 and C# work. It is
not the B2 game-state model: B2 defines its own live party, map, object and
combat state with normal C ownership. The emitted B1 16-bit code still reads and
writes raw bytes through the guest memory; every hand-written B1 consumer goes
through this header.

## Rules

- Fixed-width fields only (`uint8_t`, `uint16_t`, `uint32_t`); no pointers,
  `int`, `long`, `size_t` or host padding inside a record.
- The offsets are pinned with `_Static_assert`, and the sizes are checked on
  both Linux and Windows x64 (`tests/c/windows.sh`).
- Records are little-endian, like the original and like every protocol message.
  `yt2_party_block_parse` / `yt2_party_block_write` are the only places that
  copy file bytes into a struct (`memcpy`), and they check the signature and
  size first.
- The byte at a party signature's offset 0 is **not** part of the string: a
  written save has `00` there, while the game's static DGROUP template (what an
  untouched `CURGAME` holds) has `'S'`, so the same slot reads `"SMITHWARE
  PARTY"` there and `"MITHWARE PARTY"` at offset 1 in a save. The parser checks
  bytes `01h..0Fh`.

## Save file (`CURGAME`, `SAVGAMEn`)

The game's save routine (`swreg.asm sub_0F823`) rebuilds the party block from
DGROUP:944Fh and copies the other six blocks from the current `CURGAME`
unchanged. The file is 77,509 bytes (`0x12EC5`), seven contiguous chunks:

| offset | size | records | name | contents |
|---|---|---|---|---|
| `00000` | `01388` | 1 x 5000 | party | one 500-byte header + nine 500-byte roster slots |
| `01388` | `03840` | 144 x 100 | objects | runtime object records |
| `04BC8` | `0AC20` | 16 x 2754 | levels | one block per dungeon level |
| `0F7E8` | `00284` | 1 x 644 | world-table | world descriptor table |
| `0FA6C` | `00260` | 1 x 608 | world-index | world index |
| `0FCCC` | `00139` | 1 x 313 | world-map | overhead map |
| `0FE05` | `030C0` | 1 x 12480 | cache | working cache |

The chunk table is exported as `yt2_save_chunks[]`; its constants (`YT2_SAVE_*`)
come from the sizes/offsets the game's file-struct initializers use (`fn_18312`,
`fn_1838A`, `fn_182F4`, `fn_1834F`, `fn_1836E`, `fn_183A4`). The four
world/cache blocks are stored as sized byte records until their internal fields
are needed; the party and object record sizes are part of the format.

## Party block (`0x1388`)

The first 5000 bytes of the save are DGROUP:944Fh..963Ah. The first 500 bytes
are the game-state header, the remaining nine 500-byte slots are the roster (the
party is the non-empty subset; `SAVGAME1` from the M5 gate has the active CONAN
in slot 0 and the four recruitable characters in slots 5..8).

### Header (500 bytes)

| offset | field | notes |
|---|---|---|
| `000` | `signature[16]` | byte 0 `00` (save) or `'S'` (static template), then `MITHWARE PARTY` + NUL; a typed save name overwrites from byte 0 and vanilla loads it anyway (no check) |
| `020` | `explore_flags` | `v_946F` explore/panel flags (boot 0x2000; skill bits ORed by `fn_0ABEC`) |
| `046` | `mean_mapping` | party mean mapping (`v_9495`, `fn_0ABEC`, DGROUP-live) |
| `048` | `mean_navigation` | party mean navigation (`v_9497`) |
| `04A` | `mean_survival` | party mean survival (`v_9499`) |
| `052` | `music_id` | music track (`v_94A1`; boot 0x0C) |
| `086` | `sound_state` | music/sound present+enabled bits (boot 0x8000 no-sound; load clears bit 15 via its fixups) |
| `088` | `game_speed` | 5 in the shipped state (`v_94D7`) |
| `096` | `facing` | `4000h` south |
| `098` | `x` | party tile coordinate (`v_94E7`) |
| `09A` | `y` | party tile coordinate (`v_94E9`) |
| `09C` | `day` | day of month, 1..30 (`v_94EB`; embark 4) |
| `09E` | `month` | month, 1..12 (`v_94ED`; embark 11) |
| `0A0` | `year` | year (`v_94EF`; embark 0x222) |
| `0A2` | `scene` | time-of-day clock (`v_94F1`; embark 0x1A4, +1 per 91 game ticks, wraps 0x5A1 to 1 and rolls the date; see "Exploration time") |
| `0A4` | `active[5]` | `v_94F3` highlight slots (opaque words; the roster clears entries on unassign, the game sets them in play) |
| `0AE..1EB` | reserved | rest of the header snapshot |
| `1EC` | `assigned[4]` | `v_963B` assigned party, 1-based roster slots (0 = empty); kept in sync with the per-slot `0x800` flags on every roster toggle (`fn_1C354`), compacted by DONE (`sub_1C576`), cleared with the flags by new-game setup (`fn_1B9D0`); `party_ok` (`v_505E` bit `0x200`) is derived from its sum (`fn_0D36C`), never stored |

Only the fields the port and the gates use are named; the reserved spans keep
the layout exact. The DGROUP labels for the rest are game-state globals, not a
second structure.

### Roster slot (500 bytes)

| offset | field | notes |
|---|---|---|
| `000` | `name[14]` | ASCIIZ |
| `00E` | `class_id` | 1..9 (creation order) |
| `010` | `sex` | 1 male, 2 female |
| `012` | `portrait_pic` | group-7 picture id (`0x13` + index) |
| `014` | `portrait_index` | 0..17, even male, odd female |
| `016` | `level` | 1 for a newly created character |
| `018` | `experience` | 4-byte BCD (`fn_09ADC` adds kills into it) |
| `01A` | `field_1a` | byte (XP byte 2) |
| `01B` | XP top byte | was misread as `weight` (37 in most shipped slots); it is BCD byte 3, writing a weight here clobbers XP |
| `01C` | `status` | condition/roster/spell-school flags |
| `01E..031` | reserved | |
| `032..03B` | hidden bases | current-side combat bases (`+32h` ranged ACC, `+34h` melee ACC, `+36h` ranged DAM, `+38h` melee DAM, `+3Ah` ABS; `fn_0B3D2` copies them over, zero in creation) |
| `03C` | `current` | 64-byte stat block, current values |
| `07C` | `maximum` | 64-byte stat block, displayed/maximum values |
| `0BC..0C9` | reserved | |
| `0CA` | `spellbook[16]` | known-spell bit legs (`sub_17FD1`: leg `spell/16`) |
| `0EA..117` | reserved | |
| `118` | `carried_weight` | equipment weight; compared with `carry_capacity` |
| `11A` | `pack[8]` | 4-byte item entries |
| `13A` | `equipment[6]` | 4-byte item entries; entry 0 is `021Eh` (bare hands) |
| `152` | `effects[5]` | 16-bit effect ids |
| `15C` | `flags` | 16-bit flags |
| `15E..17B` | reserved | |
| `17C` | `container[3][26h]` | three 38-byte item/container records |
| `1EE..1F3` | reserved | |

A 4-byte item entry is `id` (`u16`) + `count/state` (`u16`); see
`yt2_party_item`.

### Creation and roster (`fn_13EC4`, `fn_1C354`, `fn_13F2E`)

The driver loops class → portrait → sheet → items → name → preview per
character; `Q` aborts any stage (the items stage leaks its slot: no wipe),
`KEEP` grants starting spells and writes the 5000-byte party block, then the
next free slot opens. Preview re-picks re-enter one stage with `v_5064` bit
`0x8000` (`RETURN`/`E` instead of `QUIT`). Rolls are `45 + fn_1797C(15)` per
attribute in STR/DEX/INT/WIS/CHA/STA order, HP is 25% STA, MP and casting are
class formulas over INT/WIS maxima, capacity is 10x STR, and the thirteen skills
derive in `sub_1426E` (verified against the M5 CONAN roll). Rating is the
attribute mean: ≤49 POOR, ≤52 AVERAGE, ≤55 GOOD, else GREAT (level 1 only). The
take list is fixed for every class (files 542/412/418/424/363/312/6/54, at most
four); drops land by item flags (ranged `0x8000` → slot 0, weapons `0x4000` →
slot 2, the rest → pack), equipment recomputes in `fn_0B3D2` (weapon skill by
`0x4000/0x2000/0x1000` flags, armor and effects add ABS). The name editor takes
12 characters (off-by-one on the 13 budget), rejects the empty name, trims
trailing blanks, and stores uppercase (the input layer uppercases `a-z`). The
portrait screen's sex label names the toggle target (`FEMALE` while male). The
roster assigns up to four slots: each toggle flips the slot's `0x800` flag and
its `v_963B..41` entry together (unassign also clears `v_94F3`; a flag with no
entry detours to the sheet, `loc_1C477`), `F1..F9`/clicks view the sheet, `D`
compacts and derives `v_505E` bit `0x200` from the entry sum; delete refuses
quest-flagged carriers (`flags & 1`, effects read stride 4) behind `ARE YOU
SURE`. Toggles and DONE are memory-only; only a deletion writes the party block,
at confirm time. B2 keeps this record in the party struct (no session copy, no
seeding) and matches the file timing exactly. Mouse tables: `66DCh` class,
`6742h` portrait, `5F84h` options, `606Eh/6098h` items, `60F4h` paperdoll
(origin 116,60), `5B58h` roster; clicks shift `+8,+8` while a carried icon is
the cursor, and the cursor window is `(1,8)-(316,195)`.

### Stat block (64 bytes, twice per slot)

The field order is the stat panel's name table at DS:540Ch; `fn_0AB61` sets
`carry_capacity` to ten times strength.

| offset | field | | offset | field |
|---|---|---|---|---|
| `000` | `strength` | | `01C` | `survival` |
| `002` | `dexterity` | | `01E` | `projectile_skill` |
| `004` | `stamina` | | `020` | `slashing` |
| `006` | `intelligence` | | `022` | `bashing` |
| `008` | `wisdom` | | `024` | `polearm` |
| `00A` | `charisma` | | `026` | `casting` |
| `00C` | `projectile_accuracy` | | `028` | `mapping` |
| `00E` | `projectile_damage` | | `02A` | `navigation` |
| `010` | `battle_accuracy` | | `02C` | `bartering` |
| `012` | `battle_damage` | | `02E` | `repair` |
| `014` | `absorption` | | `030` | `thievery` |
| `016` | `hit_points` | | `032` | `linguistics` |
| `018` | `magic_points` | | `034` | `chemistry` |
| `01A` | `carry_capacity` | | `036..03F` | max-side hidden combat bases |

In the shipped roster the two copies differ only in `hit_points` (CONAN has 4
current / 13 maximum); the second copy is what the character sheet displays.
Only the current copy's `036h..03Eh` tail is live: it holds the maximum-side
hidden combat bases (`+72h` ranged ACC, `+74h` melee ACC, `+76h` ranged DAM,
`+78h` melee DAM, `+7Ah` ABS); the maximum copy's tail is unused.

## `PICTURES.VGA`

Ten picture groups pack the whole file back to back. A picture is raw 8-bit
pixels, `width * height` bytes with tightly packed rows (no stride padding). The
groups are the picture-cache classes; the cache slot counts depend on how much
EMS is available.

| group | size | count | offset | bytes |
|---|---|---|---|---|
| 0 | 318×198 | 23 | `00000000` | 1 448 172 |
| 1 | 210×105 | 101 | `001618EC` | 2 227 050 |
| 2 | 140×155 | 215 | `00381456` | 4 665 500 |
| 3 | 190×110 | 162 | `007F44F2` | 3 385 800 |
| 4 | 224×74 | 18 | `00B2EEBA` | 298 368 |
| 5 | 224×62 | 12 | `00B77C3A` | 166 656 |
| 6 | 56×136 | 55 | `00BA073A` | 418 880 |
| 7 | 32×32 | 270 | `00C06B7A` | 276 480 |
| 8 | 16×16 | 510 | `00C4A37A` | 130 560 |
| 9 | 8×8 | 576 | `00C6A17A` | 36 864 |

The file is 13 054 330 bytes (`00C7317A`). Palettes are separate: the logo and
Webfoot intro pictures use their own palettes, the rest the main game palette.
B2 exposes a group table and a pointer to each picture; the gate pins the group
boundaries and an FNV-1a checksum of every group.

## Text face and clue book

The executable carries four 96-glyph font banks; the pointer table is at
DGROUP:`0214h` and `v_0CEC` selects an entry (byte offset 0/2/4/6). The standard
text face is bank 0 at DGROUP:`021Ch`:

- 96 glyphs for characters `20h..7Fh`;
- six row bytes per glyph, one byte per pixel row;
- bit 7 of a row is the leftmost of six columns;
- glyph pixels use the requested palette entry, unset pixels are transparent
  (the original has an opaque variant selected by `v_0B26`).

The clue-book help page (F8 in play, Tab from the main menu) is composed from
three layers and palette 0:

| layer | source | placement |
|---|---|---|
| background | `PICTURES.VGA` group 0 picture 13 | (1,1), 318×198 |
| text | bank 0, colours `0Dh`/`59h`/`08h` | title (6,4); “HELP SCREEN” (249,4); Tab line (21,24); 13 list lines (16,60), six-pixel leading |
| command bar | group 8 icons `20h,145h,147h,153h,149h,14Bh,14Dh` | (62+30n,180), 16×16, `FFh` transparent |

The page pixels use none of the animated palette entries `D0h..DFh`. The B1
frame also contains the hand cursor at (24,170); the B2 gate pins the
cursor-free composition, FNV-1a 64 `07D368FE31D7447F`.

## Start-up scenes

The start-up sequence settles on ten static compositions before play begins.
Pictures are copied onto a black 320x200 surface; the story text screens use the
standard 6x6 face. All palettes come from `WORLD.DAT`: palette 8 is the webfoot
logo palette, palette 0 is the attract/title/menu palette, and the story screens
light only entries `90h..9Fh` of palette 0.

| scene | composition | palette | frame FNV-1a 64 |
|---|---|---|---|
| webfoot | group 0 picture 16 at (1,1) | 8 | `C5B1519EF4C1133A` |
| story-sleep | text at (10,174), two lines | `90h..9Fh` | `FB816B027A864AD1` |
| story-knock | text at (34,174), two lines | `90h..9Fh` | `033C9733AF00EB97` |
| attract-combat | group 0 picture 17 at (1,0) | 0 | `1B323B732AF8CFD9` |
| attract-magic | group 0 picture 18 at (1,0) | 0 | `E7BF3FF049508B10` |
| attract-dragons | group 0 picture 19 at (1,0) | 0 | `FD9A4B6D3EA90207` |
| attract-tower | group 0 picture 20 at (1,0) | 0 | `0384E6A33033B700` |
| attract-shop | group 0 picture 21 at (1,0) | 0 | `EA4646AEFAF5EADD` |
| title | group 0 picture 22 at (1,0) | 0 | `09BEC5E9162924EF` |
| menu | group 0 picture 2 at (1,1) | 0 | `6ADE1397DAE95BC0` |

The story text is drawn as a shadow at `(x,y)` in colour `93h` and the body at
`(x-1,y-1)` in colour `98h`, six-pixel leading. The source strings are the sleep
and knock captions in the executable's DGROUP text table.

Between the statics the attract loop runs one scripted cycle per pass: boot (VBE
publisher logo, webfoot, SW logo) -> L1 -> W5 -> shooting star (53 plots, four
per tick) -> W10a -> L2 -> W10b -> L3 -> W20 -> L4 -> story scroll (32/38/128
rows at two rows per frame, palette cuts between groups instead of the vanilla
fade bursts) -> W7 -> L5 -> W45 -> W15 (e4 countdown) -> L6 -> sleep -> menu
tail -> menu-exit blank. The L-phases walk sprite pose cycles: A0 has 6 poses,
split by the copyright text into a pre-copyright bg (L1/L2 side) and a
post-copyright bg (L4 side) - the text lands mid-L3 on cycle 1; A1 (L5+W45) and
A2 (W15 tail+L6) have 3 poses each with e4 frozen. Cycle 2+ inherits the menu's
div-3 leftover, so its loops run longer (L1 17, L3 15, L4 20 multis after
wait-tail trims). The audio script per two cycles is music 3/18, the gate-star
music-service, the menu-idle music-stop, then the replayed pair. B2 ports this
in `engine/port/src/intro.c` and gates it behaviorally against B1 in
`engine/port/tests/check_intro.py` (pose sets and cycle walks, scroll rows,
audio order+ids; B1's fade pacing, wait lengths and palette bytes are host
timing and stay unbounded).

## `WORLD.DAT` map prefix

The map database is a flat prefix at the start of `WORLD.DAT`; the sound and
music records follow it. The B2 loader in `engine/port/include/yt2/world.h`
reads this prefix into typed tiles, map descriptors and map names.

The terrain and object index semantics are already reversed in the reference
project: `_ref/TalesOfYonder/TalesOfYonder/asset/YT2_TerrainDescriptors.json`
(texture class per terrain type) and `YT2_ObjectDescriptors.json` (map icon,
facing and open state per object type). The exploration slice consumes those
tables instead of re-deriving them.

| offset | block | layout | notes |
|---|---|---|---|
| `00000` | tiles | 115 200 × 4 | one 800×144 world grid, row-major |
| `70800` | map descriptors | 120 × 6 | `suffix[4]`, `name_index` byte, `type` byte |
| `70AD0` | map names | 70 × 20 | fixed-width, space padded |
| `71048` | rest | | SFX records |
| `8270A` | palettes | 9 × 768 | 6-bit RGB DAC triples |
| `8420A` | rest | | world/music records |
| `1AE0F6` | LOGO.PCX | 29214 | 640x480x256 PCX (fn_15DC2 VBE logo) |

A tile is `terrain`, `flags` (block/passthrough), `object` index and one
reserved byte. The tiles form one world grid, not per-map blocks: the engine
loads whole 800-tile rows, so map (`map_x`, `map_y`) tile (`tile_x`, `tile_y`)
is world row `map_y * 24 + tile_y`, world column `map_x * 40 + tile_x`, and a
map is a 40×24 window into the grid. Descriptor type 0 is a filler map, 1 a
world map, 2 a dungeon. The shipped file is 1 796 340 bytes; the first
descriptor is filler, map 21 is `YENDOR-PORT HOPE` and map 24 is `PORT HOPE`
dungeon level 1.

The display name the clue book and the map title use is the trimmed name plus
the descriptor suffix: `0 ` means no suffix, `0n ` reads `MAP n` (`LOST ISLES OF
YENDOR MAP 1`), anything else reads `LEVEL n` (`BLACKWING MINE LEVEL 1`).

The nine palettes are loaded with `fn_1821A` (offset table at DS:0CE6F, record
size `300h`): palette 0 is the title/attract palette (FNV-1a 64
`E3275EE3AE459FB5`), and the intro/story code fades only selected ranges of a
palette. Palettes are stored as 6-bit DAC values, not 8-bit.

The exploration palette is palette 0 with two animated ranges. `D0h..DFh` is the
4-phase rotator (`sub_100F3`, every 5 game ticks, base at DS:4C06h; see "Cursor
and palette animation"). `E0h..FFh` is a 32-entry window that steps one scene
(+3 bytes) per 91 game ticks from the palette-data record at `WORLD.DAT 0x82D0A`
(`sub_10208`, from DS:4A56h + `v_9581`); the rotation base is the same record at
offset `0x1B0` (file `0x82EBA`). Embark continues at `v_9581 = 0xED` (menu
history, M5-pinned), stepping up (`v_9583 = 3`) while `v_95E5` counts down from
`0x22`. `fn_0FE77` re-syncs the window on location changes (`v_9581 = (scene -
0x168) * 3` for scenes `0x168..0x1D9`).

## Exploration (dungeon window, room renderer, movement, time)

B2 paths: `engine/port/include/yt2/room.h`, `engine/port/src/room.c`;
`yt2p-play` embarks with the menu `E` + `ESC` flow and walks with the arrow
keys. Facings are bits: `8000h` north (`-y`), `4000h` south (`+y`), `1000h` east
(`+x`), `2000h` west (`-x`). The M5 arc stays on one dungeon level: embark
`(166,36)` west, scene `0x1A4`, through the terrain-7 doorway at `(162,31)`
(which double-steps to `(161,31)`), to the spider and the chest.

### Dungeon window (`fn_10BF6`)

The window is 78x78 8-byte cells at `v_0CFC`, origin (`v_0CF6`, `v_0CFE`).
`fn_10BF6` recomputes the origin as party minus `0x27`, clamped to `[min - 15,
max - 15]` per axis (bounds `v_519E/v_519C = 0x28/0x2F7`, `v_51A6/v_51A4 =
0x18/0x77`), then reads 78 rows: 800-tile `WORLD.DAT` grid rows (`fn_1854A`, 4
bytes per tile) plus the save visited bitmask (`fn_1838A`, 100 bytes per row). A
cell is `terrain`, `object`, `misc` (object-record id) and `flags` (`0x8000`
visited, `0x400` has object, `0x6000` door/special, bit 0 hidden). The party
cell pointer is `v_506E = (y - origin_y) * 0x270 + (x - origin_x) * 8`. After
every move the engine re-renders and reloads the window only past the 9-cell
inner margin (origin `+9` / `+0x45`); `fn_1203A` marks the current plus the
ahead row/column visited (two rows facing north/south, two columns east/west)
and persists the bits (`fn_11FCC`).

### Terrain and object tables

DGROUP:`0E5B3h` holds 62 12-byte terrain rows: `+0` floor picture (group 4),
`+2` ceiling picture (group 5), `+4`/`+6`/`+8` wall pictures/params, `+0Ah`
overhead icon. Terrains 2..15 are walls (floor = ceiling = 0), 16..57 open
ground, 0..1 void; the wall faces come from `PICTURES.VGA` group 1 (210x105).
DGROUP:`0E1D7h` holds 68 10-byte object rows: four facing pictures (group 1)
plus the overhead icon at `+8`. The picture cache classes (`yt2_picture_class`,
DS:`7834h`) map cache id to group: `0x10` group 1, `0x20`/`0x30` groups 2/3
(object sprites), `0x40` group 4 floor, `0x50` group 5 ceiling, `0x60` group 6
overlays, `0x70` group 7 portraits, `0x80` group 8 icons/arrows/cursors, `0x90`
group 9 budget icons.

### Perspective table and column blitter (`fn_1A0A9`)

The column geometry is a 0x499C-byte table at `WORLD.DAT 0x19C826` (`fn_18268`),
with nine sub-table bases set at boot (`v_516C..v_5180` =
`0/0x386/0xBF2/0x13B6/0x3B76/0x3CA8/0x3DF2/0x41D2/0x4316/0x4460/0x4858`).
`fn_1A0A9` dispatches on `v_50B4`: 0 front faces, 1 floor, 2 ceiling, 3/4 side
walls, 6 far-wall strips (7x113 from group 6), 7 near objects, 8 far objects,
`9..E` overlays. `v_50C8` is the perspective column, `v_50C2` the distance shade
added per pixel and clamped to its 16-entry palette row (`sub_1ABED`, skipped
at/above `0xD0`), `v_0CCA` the picture, `v_0CCC` the cache class, `v_0B26`
nonzero for `FFh` transparency, `v_5120` an optional clip, `v_50CE` a source
offset (mode 6). Entries are 6 bytes (`x`, `y`, strip-list pointer; screen
offset is `y * 320 + x` via `sub_1AC1B`) and strips are 6-byte (`reps`, `run`,
`skip`) runs copied by `sub_1AB23` (horizontal) and `sub_1AB91` (vertical); mode
0 additionally honours the `v_5060` sprite shade table and the `v_5120` clip.

### View gather and passes (`fn_10E42`)

`sub_1152A` gathers 51 columns into the view line at DGROUP:`0x6D66` (8 bytes
per column, same layout as window cells): groups of 17/17/5/3/3/3/3 cells
walking the lateral axis, stepping one depth row per group from six cells ahead
back to the party row, per-facing. `sub_11620`/`sub_11718` mark hidden columns
(solid range `v_5134..v_5132`, pointer lists at DGROUP:`8` and `0xDA`).
`sub_10F53` blits the full ceiling then the full floor picture (`fn_19E12` at
`(8,8)`/`(8,70)`) with the 62/74-row gradient postpass (`fn_1A590`, bands shaded
with `v_5082..v_508E`), then the ceiling/floor detail columns (`sub_10EB2`,
modes 2/1) where a neighbour picture differs (`sub_10EA0` pairs even/odd
pictures). `sub_111DB` draws the wall faces per column (front mode 0, sides
modes 3/4, far group mode 6 via `sub_1143B`), tile objects (`sub_113AB`, mode 7
in the `v_0CD6..v_0CDC` id ranges else 8) and runtime objects (`sub_114DC`,
`0x400` cells), plus the three records at `0x51BC` in combat (`sub_1150F`).

`fn_17D5A` selects the seven `v_5082..v_508E` shades from `v_9469`
(`0x76D0`/`0x7732`/`0x7740`) or the scene table at `0x722E` (32-byte `[lo,hi]` +
two shade sets), brightens them from `0x76DE`, and builds the 63-word column map
at `0x774E` (read by the minimap draw).

### Runtime objects

Eighty `0x9C`-byte records live at DGROUP:`0xF20`. `fn_12D72` claims an empty
slot and loads the 106-byte definition (`WORLD.DAT 0x1A792A` via `fn_1850E`,
index `0x1A92D6` via `fn_1852C`); `fn_12E82` finds a record by id; `sub_136FF`
culls past column `0x21..0x2C` by `+0x94` flags. `sub_11078` draws the sprite
(group 3 if `+0x92` bit 0 else group 2, mode `+0xA`, transparent, clip `+0x18`,
animation from `+0x4C`), a second layer (`+0xC` bit 4, picture `+0x1A`), and a
group-6 overlay (`+0xC` bit 3) at mode-dependent positions with picture
`v_5188`/`v_5186`/`v_5184`. Taken/killed state is a bit per object id in the
save world-map chunk (`sub_12F48`/`fn_12F8F`). Stepping onto an occupied cell
picks the record up (`loc_012D4`: moves it to `0x5258`, clears the cell). After
every step `fn_13056` walks each monster up to 5 cells toward the party
(`fn_00ECD` collision) or attacks (`fn_12EA0` + `fn_13420`).

### Explore UI

`PICTURES.VGA` group 0 picture 1 is the explore backdrop (viewport frame,
chapter panel, icons, message side panel, arrows, portraits); scenes save it to
a scratch buffer (`fn_12691`) that `fn_126DE`/`fn_1270C` restore per frame
before the dynamic bits. `fn_12602`/`fn_1274F` draw the four portraits from
layout `0x61C8` (rect, three status icons from roster `+0x1C` bits, buff icon,
three `0x26`-wide 5-row bars via `sub_12A06` with HP at `+0x52`/`+0x92`, the
`fn_1A03C` glyph or group-9 picture `0x15`, and `DEAD` from `0x7AB1`).
`fn_11754` prints the compass (` NORTH`/` SOUTH`/ ` EAST`/` WEST` at `0x78D4`)
when `v_946F & 0x400`; `fn_11836` fills the 9x7 minimap buffer at `0xD00` when
`v_946F & 0x4000` (drawn top-right with per-cell `0x774E` shades). `v_946F`
`0x1000`/`0x2000`/`0x4000` select the combat/explore/map panel (`fn_1755C` + 9
group-8 icons from layout `0x6372`, `fn_1270C`, `fn_11836` + `fn_117AC`). Arrow
feedback flashes group-8 pictures through the `v_50A0` tables at
`0x6420..0x6452` (`sub_013DB`).

### Movement (`fn_00FBA`)

Up/down/left/right (`0x48`/`0x50`/`0x4B`/`0x4D`, `0x73` strafes) set the
`v_0B9C`/`v_0BA0`/`v_0B9E` step from the facing; left cycles
`8000h->2000h->4000h->1000h`, right the reverse. Bounds are `x 0x28..0x2F7`, `y
0x18..0x77`. The destination cell decides: `+6 & 0x6000` prints the door message
(`fn_11914` record lookup in the `v_0CF4` door table via `sub_11A93`, key/item
checks, text from `0x7BBA..` at `(240,96)` via `fn_0784D`) and blocks; terrain
`6..11` (`v_5146`/`v_5144`) plays the step animation and moves twice
(`sub_013FF` + the shared tail, verified: the west step through terrain 7 at
`(162,31)` lands on `(161,31)`); otherwise `fn_00E6C` blocks void `0..1`, walls
`2..5`/`11..15` and `58+` (code 2 is a silent bump) and `fn_00EA6` blocks
objects `0x21..0x23`/`0x25`/`0x27..0x2A`, else the party moves one cell with the
step sound (`v_5142`) and bump sound (`v_5148`). Post-move order is pickup scan,
`fn_1203A`, monster AI, action cells (`fn_09F1C`: table `0xD22B` matches object
ids `0x11..0x14`/`0x3F..0x43` to actions, `0x4000` teleports), render,
portraits, edge reload.

### Exploration time

`v_94F1` is the game clock, serviced by the INT 1Ch ISR alongside the D0-DF
rotator: with `v_50F6` bits `0x4000`/`0x2000`/`0x1000` set, countdowns B/C/D
(`v_50EA`/`v_50EC`/`v_50EE` = 91/91/5) fire `fn_10073` (`+1` scene, wrap `0x5A1`
to 1 rolling day/month/year, re-init at `0x168`/`0x438`), `sub_10208` (E0-FF
window step + `fn_17D5A`), and `sub_100F3` (rotation). Explore mode runs
`0x7000`; menus, combat and chest UIs clear `0x6000` (clock and sky pause,
rotation continues). Resting adds `0x3C` per round (`loc_0E892`). M5 pins the
schedule: gates on at embark (~tick 3972, `fn_0FF27`), first fire at tick 4063,
E0-FF at `v_9581 = 0xED` stepping `+3` (menu history, M5-pinned), room frame
(tick
4165) at scene `0x1A5`+1 fire with E0-FF window file offset `0x82DFA`.

### Embark and the in-game menu (`fn_0EBD6`)

Boot (`fn_1B9D0`) rewrites `CURGAME` fresh from the template and never reads it:
it zeroes `v_963B` and the slot flags, sets positions, date, scene, speed and
music, then writes the block plus zeroed chunks. B2 boots the same bytes
(gated). Menu `E` with a party (the gate derives `v_505E` bit `0x200` from the
`v_963B` sum, `fn_0D36C`) runs the skill averaging (`fn_0ABEC`), stops the music
and returns from the menu into the in-game menu; an empty party buzzes.

The in-game menu offers game speed (`A`, cycles 5/1/9), quit to DOS (`D`,
confirm 1), music (`M`) and sound (`U`) toggles, new game (`N`, confirm 31, back
to the menu), save (`S`) and load (`L`) slot lists, and continue
(`R`/`ESC`/right-click). Each option carries a `v_505E` enable bit; fresh boots
get mask `0x9D`, boots with saves on disk get `0x71` (load/new instead of
save/speed/quit/continue), the in-explore `D` panel gets `0xFE`. `O` is a no-op
(`fn_00000` always returns 1). Continue leaves for `fn_0F2C0`: window load,
neighbour probe, backdrop save, portraits, full render, then `fn_0FF27` enables
the clock gates.

`CURGAME` is session scratch: roster, DONE and embark write nothing except the
visited bits (`fn_1203A` persists one objects-chunk byte per touched row through
`fn_11FCC`; entering explore writes exactly three). Persistence is the six
`SAVGAME` slots. Saving (`sub_0F823`) strcpy's the typed name over the header
start, refreshes `CURGAME`'s party block from live state, then copies `CURGAME`
over the slot file. Loading copies every chunk back except objects (explored
bits stay) and applies the sound-state fixups. The boot scan (`sub_01D84`) fills
the slot list from the files' first 25 bytes. B2 matches all of it byte for
byte.

### Chest search and loot (`fn_0795E`, `fn_073E3`)

`SPACE` search picks `v_5560` (`0x6ED6` faced cell, `0x6EEE` party cell) and
shows prompt 5/6 (`fn_0A834`, VGA-direct over the `fn_06F86` wipe); `N`/`ESC`
cancel (`ax` 0, wipe, explore repaints `SOUTH`), a portrait click picks the
opener. Open (`sub_07B1D`) plays SFX 6 and `++[v_5560]+2`, aliasing
gathered-view column 46/49's object word, so the chest picture flips 4→5 while
the modal is up (close `--` flips it back). The modal dispatches regions
`0x5ABC` (loot `0x63CE`, quick access `0x6372`, portraits `0x61C8`). Currency
takes (`fn_070EA`) present, VGA-clear and paint the 2-line receipt; item takes
repaint the loot and run `fn_11F83` (gated clear no-ops, name paints, stale
compass stays). Quick access drops (`fn_176EC`) refuse `0x2000` items (buzz,
screen untouched) else clear (consuming bit `0x100`) and stay silent; portrait
drops (`fn_08D3F`) and loot drop-backs (`loc_072E7`, weight and count checked)
match, the drop-back repainting the name. Side panel writes split across
`fn_06EA4` (WorkSeg+EMS iff bit `0x100`, consumes), `fn_06F86` (VGA direct, no
flags) and `fn_076AC` (paints iff `v_5060 & 0x80`, sets `0x100`); the compass
repaints only in explore (`fn_11754`, iff `v_946F & 0x400`). Taken masks and
unlock bits persist per chest index in the save.

Explore mouse shares the `0x5ABC` regions: the arrow buttons (`0x6420`) drive
`fn_00FBA` through KeyCodes `0x48/0x4B/0x4D` (forward/turns) plus the mouse-only
strafes `0x73/0x74` (lateral step, flash pics `0x18/0x17` and `0x1C/0x1B`), with
the anim-terrain double step in the shared tail; viewport right-click runs the
same `loc_003F3` search as SPACE (party cell, else faced cell); the quick access
pane repaints the currency trio (`fn_1765A`: labels plus purse BCDs at rows
96/102/114/120/132/138, SFX 2, no gather). Viewport left with an empty cursor
does nothing; icon row, portraits and the message link open their own UIs.

## Clue-book maps (F1)

F1 from the help page opens the map catalog; Enter on a map opens its overhead
view. Both screens use palette 0 and the standard 6x6 face.

The catalog list is the executable's table at DGROUP:`0E95Ch`: a count word
(`0x37`) and 55 DWORD entries in reading order, each a one-based map descriptor
index and a flags word. Five entries carry flags bit 15 (ranks 30..32, the PORT
HOPE town maps, and ranks 53..54, two Yendor sea maps); the bit gates nothing in
the clue book (every rank opens its overhead view, verified on the original).
The page is group 0 picture 14 at (1,1), the `DARK UNION` / `MAPS` header, a
fourteen-row window of display names at (40,27) stepping 10 pixels (the selected
row in gold `8Ah`, the rest `0Ah`), the `MORE` arrows at (129,167) and the seven
command icons at (62+30n,180). The B2 gate pins the cursor-free composition for
map 67 (`ANCIENT RUINS`, rank 0), FNV-1a 64 `064C1427810F47E3`; the B1
framebuffer is that composition plus the 103 hand-cursor pixels at (24,170), and
the captured palette is palette 0 except one byte of the animated `D0h..DFh`
range.

The overhead view reads two more `WORLD.DAT` records, named by the game's record
table (dword offsets at DGROUP:`D201h`, word sizes at DGROUP:`D21Dh`):

| record | offset | size | layout |
|---|---|---|---|
| 1 | `155918` | 144 × 100 | one bit per world tile, MSB first; a set bit draws the tile |
| 4 | `165890` | 405 × 8 | legend entries: 1-based `map`, global `x`, global `y`, text `line` |
| 5 | `166538` | 167 × 26 | legend text lines (referenced 1..166) |

The legend record's last three slots hold the digit-run prefix of the following
text record, so their map words never match a descriptor. The screen is the
display name at (0,1) and `SELECT LEGEND OR ESC` at (201,1) over black, then
every tile of the 40×24 window at (8×`tile_x`, 8+8×`tile_y`): a set bit draws
the terrain map icon and then the object icon (8×8 group 9 pictures, the object
masked with `FFh` transparent), a clear bit draws the void icon `13h`; legend
markers of the open map draw icon `73h`. The icon tables are the executable's at
DGROUP:`E5B3h` (62 terrain entries, 12 bytes each, icon at +`0Ah`) and
DGROUP:`E1D7h` (68 object entries, 10 bytes each, icon at +8). The B2 gate pins
map 67's composition, `725194639EB33B4F`, and a replay of the game's own
1018-icon B1 draw trace reproduces it with zero differences; the DOS original
presents the same full map (the B1 run shows only the first icons, a B1-only
presentation artifact).

Clicking a marker shows its legend (`sub_03F6B`): icon `73h` at (161,0) and the
line's 18 text bytes at (170,1) in color 123 over black (opaque cells, so a
shorter line erases the previous tail). The hit box is the 8×8 icon plus its
inclusive far edge (9×9), first legend entry wins; a miss changes nothing. B2
pins map 67 with line 116 (`BONES OF TRAVIS`, `40221FD89774C067`) and line 30
(`ENTRANCE`, `025C0FD8B4677669`).

Clue-book pointer input (`fn_PollInput` + `fn_02892`, verified on DOS): a single
left press selects the row under it; a second press within 6 ticks opens it
(Enter-equivalent, same row rules); the stale press latch then re-fires once on
the next tick (observable when a double-click opens a map whose marker sits
under the cursor). `MORE` arrows page the window by 14 (down clamps to the last
row); icon clicks switch pages; detail footers return to their lists. Left/Right
and PgDn/PgUp are ignored on lists (B1 answers PgDn, a B1-only quirk). B2 models
the latch, the phantom press, and the paging in the player, all driven by the
input gate.

## Monster statistics (F2)

F2 from the help page opens the monster list; it shares the F1 list geometry
(frame, fourteen-row window at (40,27), `MORE` at (129,167), icons at
(62+30n,180), gold `8Ah` selection) with its own header, rows and icons.

`WORLD.DAT` at `1A7994` holds 61 fixed 106-byte monster records: two 13-byte
name fields (12 characters plus NUL each; long names like `CARNIVOROUS SPIDER`
span both) and 80 bytes of detail-view statistics. The list shows the joined
names byte-sorted (all names are plain A-Z and space, so any `strcmp` agrees);
no order table exists in the executable. The page header is `MONSTER STATISTICS`
at x=208 and the icons are `20h 146h 147h 153h 149h 14Bh 14Dh` (the gold frame
follows the person's section icon, as `21h` does the map's on F1). The B2 gate
pins the rank-0 composition `D5B4C34273DCD369` and the rank-60 composition
`6E3F491525EA591E`; both reproduce their B1 framebuffers with the hand cursor
drawn at (24,170).

Enter on the list opens the monster's detail view (`sub_04022`): the name left
and `MONSTER STATISTICS` at x=208 on the title row, the animated portrait over
background picture 13, four BCD big-endian counters (`EXPERIENCE:` at (185,16)
from stats+62, `GOLD:` at (221,22) from +50, `MAGIC ORE:` at (191,28) from +58,
`NUORE:` at (215,34) from +54; gold `8Ah` values at x=251 with thousands
separators, blank when zero), seven plain word statistics
(`HEALTH-`/`ACCURACY-`/`DEXTERITY-`/`ABSORPTION-`/ `DAMAGE-`/`RANGED
ACC.-`/`RANGED DAM.-` from stats words 2/4/5/6/7/12/13; `59h` values at x=275,
blank when zero), ten `IMMUNE` flags for stats word 37
(`POISON:`/`DISEASE:`/`PARALYSIS:`/`FREEZING:`/`HEXING:`/
`CURSING:`/`FIRE:`/`COLD:`/`ELECTRIC:`/`POWER:` testing bits
15/14/13/12/11/10/3/2/1/0), `MAGIC DAMAGE:` at (185,154) and `PHYSICAL DAMAGE:`
at (167,160) showing `RESISTANT` for word37&10h/word38&3A00h and word38&0C000h
respectively, and the green `SPECIAL ATTACK-` line at y=169 with the pink
abilities string at x+90 (x is 171 for strings of up to 8 characters, else
(35-len)*6; the 61 strings are frozen from the oracle). Labels are `0Ah`, flag
values red `A7h` at x=263; the footer is `d LIST` at (11,185) with the same
seven icons as the list.

Portraits animate from the record's word 0 (the base picture): six pictures
cycle or ping-pong (eleven monsters ping-pong), then a two-picture excursion
(base+7, held base+8) fires about every 360 ticks (300 for rank 14); base+6
never shows. Group-3 portraits fill the 190x110 box at (6,33); group-2 portraits
(140x155) blit unclipped at (8,7) and overdraw the title where their art is
opaque (the centaurs). Every portrait passes through a per-monster high-nibble
remap (fitted at 1.0000 consistency for all 61; the low nibble passes through,
`FFh` stays transparent). The B2 gate pins six cursor-free excursion
compositions and reproduces all 61 B1 excursion framebuffers with the hand at
(24,170); the player animates scene 12 from the nominal phase model (5-tick
steps, restart after each excursion).

## Spell information (F3)

F3 from the help page opens the spell list over the same shared geometry.
`WORLD.DAT` at `1AA65E` holds 105 fixed 80-byte spell records: a 21-byte name
field (space-padded, NUL at byte 21; the longest names, `INFINITE ILLUMINATION`
and `SELF-INFLICTED WOUNDS`, fill it) and 58 bytes of spell data. The list order
is the original's own frozen quirk: alphabetical except that record 87 (`BLOCK
OF POWER`) appears twice (ranks 5 and 11) while record 86 (`BEAM OF DEATH`)
never appears, so 105 ranks cover 104 distinct spells. No order table with that
sequence exists in either binary; the order was matched rank by rank against the
B1 frames (a 105-Down run, every selected row template-matched against the
port's own font at x=40), and the DOS first page shows the same doubled ranks.
The page header is `SPELL INFORMATION` at x=213 and the icons are `20h 145h 148h
153h 149h 14Bh 14Dh` (the gold scroll, matched exactly against group 8). The
last page holds ranks 98-104 (seven rows, `TORNADO`..`VOLCANIC ERUPTION`); rank
98's page turn presents one transitional frame (stale rows 12-13) before the
settled one. The B2 gate pins the rank-0/14/98/104 compositions
`336AED2DF4355F25`, `4E0021E2C76A7281`, `14A80E5E78EF674E`, `EB4FF720BE73034E`;
plus the hand at (24,170) they reproduce the B1 framebuffers `CFDBBECC9F9F16B5`,
`2429157AFD25AAC1`, `EE0BBBD290210786`, `C4B3A494D5A4A386`.

Enter opens the spell's detail page over picture 13: the name left and `SPELL
INFORMATION` right on the title row, the class block (`CLASS:` at x=44, `LEVEL:`
at x=104, one row per casting class at y=34+6k with the name, level and
`SCROLL`/`TRAINING`/`CREATION` type in `0Dh`/`0Dh` and `CAh`/`8Ah`/`A7h`), the
`MP:`/`NUORE:`/`ORE:` costs (record words 1-3, gold at x=236), `AFFECTS:` with
scope at x=92, middle at x=116 and target one cell past the middle, `WHEN:` with
the value at x=74, up to five red `EFFECT:` lines at x=44, and the `d LIST`
footer with the list's own seven icons. The rows are the original's fixed
per-spell assignment (1-6 rows; two spells diverge from the F4 membership),
captured row by row from 105 B1 detail screens, as are the AFFECTS/WHEN/EFFECT
strings; the EFFECT face adds eighteen private symbol glyphs lifted from the
executable's second font bank at `2233Ch`. The B2 gate pins seven detail
compositions (ranks 0/3/31/60/85/101/104); plus the hand at (24,170) they
reproduce the B1 framebuffers, and all 105 ranks match their B1 framebuffers
byte for byte. Enter on an F4 class-detail row opens the same page for that
spell's file index (file 86, `BEAM OF DEATH`, has no F3 rank but opens from the
mage list) with the F4 icon bar; the footer returns to the class list.

The EFFECT source is white-box: `sub_03988` draws the labels (one `fn_13E8C`
call over `AFFECTS:` - the blob's double NULs become the blank rows), the costs
from `v_5A6E/70/72`, the six class rows by scanning the per-class spell tables
at `0D275h/0D28Dh`, and AFFECTS/WHEN by bit-testing `v_5A9E/5AA2/5A74/5A9C`
against the fixed exe vocabulary. The EFFECT lines come from WORLD files 6+7
(`fn_180D0` chunk table at `0D201h`): file 6 is the 4-byte `(start, count)`
index at `16762Eh` (slot `file+1`), file 7 the 39-byte text records at
`167822h`. The index chains perfectly but the text area was never authored - it
holds script bytes, item names and a stray `ER2\BK1_CH2\GAME\DAD\WORLD.DAT` dev
path, which is exactly what vanilla renders (frozen bytes are matcher-canonical:
e.g. true record byte `03h` renders a glyph the port stores as `83h`; the frames
match). Note the listing trap: segments are byte-aligned, so a runtime string
pointer is the listing offset plus 7 (`tools/port/program.py`).

YT3's WORLD.DAT holds the same structure twice, with REAL descriptions: copy A
(index `3D4AEFh`, text `3D4C9Fh`, records 0-61 clean, then truncated
mid-record-61 by copy B) and copy B (index `3D5631h`, text `3D57E1h`, all 354
used records clean English). The two indexes are identical (0/108 slots differ);
the YT3 executable is `REGISTER.EXE` (`SW.BAT` runs it as the game program) and
it points at copy B. YT3 has 107 spell records at `41B5BFh` (same 80-byte
format, record 0 identical to YT2's); its ERROR slots 59/71/80 still hold the
BLOCK OF ICE/FIRE/ ELECTRICITY descriptions. `tools/yt3_spells.py` dumps either
copy per file.

97 of YT2's 105 spells name-match YT3's, so YT3 supplies an optional description
patch (`tools/yt2_spelltext.py`, which rewrites WORLD files 6+7 only - file 7
has no other reader). Description numbers map to record words: damage to word
12, beneficial magnitudes to word 6, ailment magnitudes to word 8 (each proven
by single-candidate spells and phrase templates); YT2's own numbers are
substituted, including two stale YT3 texts (HARDY PARTY prints 650 for a 600
stat, ERADICATE 400 for 480). The official table covers 92 spells; five
duration-bearing spells keep vanilla bytes (durations live in no record word) as
do the eight YT2-only spells. `--complete` fills those 13 from curated lines
(BLOCKs from the ERROR slots, durations assumed, five spells invented from the
YT2 records). Proven live: B1 with the patched WORLD.DAT renders ACID RAIN's
substituted 145 glyph-exact (2592/2592 pixels, diffs only in the EFFECT bands).
The port renders either table with `--spelltext` (`yt2_spells_effect_lines` + a
32-entry canonical map that aligns every vanilla record byte with the frozen
table - zero conflicts - so a vanilla world stays pixel-identical); both play
scripts take `--spelltext` / `--spelltext-complete`.

## Magic users (F4)

F4 from the help page opens the class list over the same shared geometry. The
executable carries three tiers of nine class names (ten bytes padded, NUL at
byte 10) at `29625h`, `2A0D7h` and `2A13Ah`; the list shows the six magic lines
(base classes 3-8, fighter / merchant / rogue stay out) as `TIER1 / TIER2 /
TIER3` triples. The page header is `MAGIC USER INFORMATION` at x=183 and the
icons are `20h 145h 147h 154h 149h 14Bh 14Dh` (the gold book, matched exactly
against group 8). With six rows on one page the game draws no MORE line at all.
Enter opens the line's spells: byte-sorted like a true sort (the mage list holds
BEAM OF DEATH in its sorted slot, the druid list holds BLOCK OF POWER exactly
once - the F3 quirk is F3-screen-specific), counts 52/50/32/47/47/31, fourteen
rows per page with a `d LIST` footer at (11,185) in `0Fh` (`d` is the font's
left arrow). Which spells each line casts is the original's fixed assignment,
captured spell by spell from the B1 detail screens; no per-spell mask field
yields it. ESC from either F4 screen returns to gameplay (the demo maps it to
the previous scene). The B2 gate pins the list compositions `635F5999F0C8700A`
(rank 0) and `3E56F34BA5FD770A` (rank 5) plus each detail's first/last ranks;
plus the hand at (24,170) they reproduce the B1 framebuffers `7AFA214601883F62`
and `55F1BAF7B6BD4662` (list) and `76A5453A840960FC`/`42B33D023F962D96` (monk),
`0138C759C66C1856`/`2564FE0E08998DAF` (alchemist),
`63C1B75D46B91767`/`E367835A282778DF` (paladin),
`CB42493F0DF415A0`/`63005E54BE975F27` (mage),
`72DDA2EAD6DF54DE`/`C71821B1C30286E1` (druid),
`A85D9A37F1537BA3`/`9FB6858F0C9BEF02` (marksman).

## Inventory items (F5)

F5 from the help page opens the eight-row category list (no MORE line): header
`INVENTORY ITEMS` at x=225, icons `20h 145h 147h 153h 14Ah 14Bh 14Dh` with the
gold chest at slot 4. `WORLD.DAT` at `71138h` holds 744 fixed 58-byte item
structs: 19 stat bytes (param index, adds offset, BCD value, icon, weight,
flags, fits) followed by three 13-byte names. Six categories open item lists
(counts 41/52/43/13/27/24); which of the 744 records each shows is the
original's fixed assignment (no stat byte, pair or triple yields it), captured
item by item, and the orders are captured too - mostly alphabetical with quirks
(a swapped neighbor pair in armor, weapons and scrolls, SAPHIRE trailing the
jewels). Rank 27 of scrolls is file 736, not 576: both are named `SCROLL OF
FORGERY`, and the v_5110 selection register disambiguates. Lists show `INVENTORY
ITEMS` at x=6 on the left with the category right-aligned to 315 on the right
(the shared composer grew a title override for this), plus the `d LIST` footer.
The other two categories are fixed info screens over picture 13: attribute
enhancers (six icon lines at y=47+18k, keyword `CAh`, `PERMANENTLY ADD` white,
amount `59h`, target `A7h`) and transportations (three creatures with
value/uses/time lines; creature records in the exe at `2946Ch` hold value/10000,
uses and the time window, and a fourth record, FLYING RUG, is never shown). The
B2 gate pins the list ranks 0/7, each detail's first/last ranks and both info
screens; plus the hand at (24,170) they reproduce 16 B1 framebuffers, and both
info compositions also match B1's own cursor-free ESC transitional presents.

Enter on an item row opens its page (mains fn_02D9E/02E27/02E9B): picture 13,
the names joined at (6,4), the category header, the group-8 icon at (68,41)
(struct+8, +1 with flag `1000h`), `BASE VALUE:` (BCD with commas) and `WEIGHT:`
(tenths, `.6` keeps no leading zero) at x=157, `FITS IN-` (bit-driven
`BACKPACK`/`BOX`/`BAG` or `CHARACTER PANEL`/`ANY PANEL`), then per category:
armor gets `ABSORPTION-` plus `PROTECTIONS:`/`ADDS:` lists, weapons get
`DAMAGE:`, `SKILL:` and `2-HANDED:`, the rest get one conditional row
(`DURATION-`, `HEALTH-`/`MAGIC-`, or per-file quirks for files `11h-1Ch`,
`35h-45h` and 575, all matched on the 1-based selector). Type stats come from
three WORLD tables selected by the flag word (`7C81Eh`/`7DCD2h`/`7D502h` plus
the struct+0 byte offset); the protection/adds entries from `7BD2Eh` plus the
struct+2 offset, with names from the exe tables at `7B34h` (9) and `7DCAh` (27).
Armor and weapons pages offer the +N row (group-8 faces `155h+2k`, selected +1,
at (21+26k,141)): box N shows record file+N, enabled while consecutive variants
keep the parameter-row continuation bit. The footer returns to the list (ESC
reaches gameplay in the original, the list in the demo). The B2 gate pins nine
page compositions across the six categories plus a +5 state; plus the hand at
(24,170) they reproduce their B1 framebuffers.

## Complete walk through (F6)

F6 from the help page opens the 31-page walkthrough: header `COMPLETE WALK
THROUGH` at x=188, icons `20h 145h 147h 153h 149h 14Ch 14Dh` with the gold quill
at slot 5. `WORLD.DAT` at `159158h` holds 31 pages of 25 lines of 51 bytes
(fifty characters plus NUL, space-padded), drawn verbatim at (10, 23+6r) in
`0Dh` - percent and tilde render literally (the font's % glyph just looks like a
quote mark), and the last line of every page is its footer, typos included
(pages 20-31 read "N 0F 31"). PgDn/PgUp turn pages (arrows and Enter do
nothing); a nine-page ordering-info document follows in the same format but no
clue-book screen shows it. The B2 gate pins pages 0/1/14/19/30; plus the hand at
(24,170) they reproduce the B1 framebuffers `687B3AA93D1BEBEC`,
`82CDA8EC351D07E3`, `80161B205CE19807`, `21724493C02D833A`, `5605EEBBEFD64001`,
and pages 0-1 also match B1's own cursor-free ESC transitionals.

## Cursor and palette animation

The B2 screen composers render static, cursor-free pages; the pointer and the
animated palette entries are dynamic state (input position, tick phase) that B2
takes as inputs. Two small pure units close the gap to the oracle frames.

The cursor image lives in DS:3FE0h, loaded by fn_SetMouseCursor from
PICTURES.VGA class 8 (block size 256 at file offset `C4A37A` = group 8), so the
cursor id is the group-8 picture index and the everyday pointer is the hand,
cursor `1Dh`. The game draws it masked (`FFh` transparent) at (v_MouseX,
v_MouseY) with no hotspot applied at draw time (the 8-pixel shift for cursors
other than 0/`1Dh` is applied when the position moves), clipping the right edge
past x=304 and stopping past the bottom row (at x=0 a cursor past the last row
overruns the 64 KB buffer, off the visible screen; B2 clips instead). The B2
gate pins two oracle framebuffers with the hand at (24,170): the catalog
`BC1E5BFF608CB41B` and the help page `7F29E8DA4ED08B2F`.

The palette rotator (sub_100F3, serviced from the tick ISR every 5 ticks)
rewrites the live entries `D0h..DFh` from a 48-byte base: four groups of four
(blues, greens, magentas, ambers), each stepped one entry forward per call, so
the sequence repeats every four steps. The base is the 768-byte WORLD.DAT record
at `82D0A` (read into DS:4A56h at startup) at offset `0x1B0` (file `82EBA`); the
record's head is the E0h..FFh sky window source and its tail is scratch the fade
engine tramples. The base's last byte is `11h`, not palette 0's `1Bh`. B1's
stream interleaves single-entry D0 messages between the full 16-entry rotation
writes, but a port trace proves the guest emits only clean 16-entry writes: the
singles are B1's present-path sampling and have no meaning on hardware. (An
older log entry called this rotation seven-phase; the observed cycle has four
states.)

## The B2 boundary

B2 speaks companion protocol v0 (`docs/protocol.md`, implemented fresh from the
frozen spec) over the folder link: `OUTPUT.BIN` (direction 0, timed YT2B
records, restarted at the 64 MiB video limit with a keyframe after) and
`INPUT.BIN` (direction 1, followed with wrap/rewind detection; the client
creates it). The engine sends `hello(0, 0)` before its first `set_mode`, answers
the client's hello once per session, and answers every keyframe request with
set_mode, the full palette and a full-frame pass.

Presents carry set_mode on size changes, the coalesced palette span, one raw
rect per changed row (`h=1`, the 2048-byte client payload limit forbids
whole-frame rects) and frame_end, committed once per present. Inbound, B2
decodes key, relative and absolute mouse motion, buttons, quit, hello and
keyframe requests; INPUT type 3 is the absolute pointer when its payload has
five bytes and quit otherwise, which is the length rule the seam, B1 and the
client all implement (the frozen v0 table names only quit there).

`yt2p-play` drives the boundary: five scenes (help, catalog, overhead map,
title, menu), a hand cursor tracked from mouse input, and the palette rotation
stepped on the engine tick (every 5 ticks, the original's countdown cadence), so
rotation phases leave the engine as palette events instead of living in the
client. Its input follows the original's shape: game keys (arrows, Enter, ESC,
F1) drive the catalog selection and the map view, a left click selects a catalog
row or the icon bar, and keys drain one per tick through a 15-deep type-ahead
ring like the BIOS queue (mouse and control always flow, so a click's position
and press stay together).

## Other records

| record | source | notes |
|---|---|---|
| `yt2_file_record` | `swreg.asm FILE_* EQUs`, DS:904Bh.. | DOS handle/transfer block and an ASCIIZ name tail |
| `yt2_picture_class` | DS:7834h, ten 16-byte entries | picture-cache class table |
| `yt2_music_header` / `yt2_music_patch` | `docs/sbfmdrv.md` 4.1/4.4 | `CTMF` music block and 16-byte FM patches |
| `yt2_sfx_entry` | DS:0CF51h | 80 dwords of `WORLD.DAT` SFX offsets |

## Checker

`yt2saveinfo SAVEGAME` prints all seven chunks, the party position and every
non-empty roster slot through the typed model. `tests/c/saveload.sh` runs it on
the save the game wrote through its own disk panel, and `tests/c/windows.sh`
requires the Linux and Windows builds to print identical output.
