# B1 data structures (executable spec)

`engine/c/include/yt2/data.h` is the typed description of the original's
**on-disk** records: the save file (`CURGAME` / `SAVEGAMEn`), the party
header and roster slots, the DOS file struct, the picture-cache class table,
the `WORLD.DAT` music header/patch and the SFX table.  This is the B1
pseudo-port's record model, useful documentation of the formats for the B2
and C# work.  It is not the B2 game-state model: B2 defines its own live
party, map, object and combat state with normal C ownership.  The emitted B1
16-bit code still reads and writes raw bytes through the guest memory; every
hand-written B1 consumer goes through this header.

## Rules

- Fixed-width fields only (`uint8_t`, `uint16_t`, `uint32_t`); no pointers,
  `int`, `long`, `size_t` or host padding inside a record.
- The offsets are pinned with `_Static_assert`, and the sizes are checked on
  both Linux and Windows x64 (`tests/c/windows.sh`).
- Records are little-endian, like the original and like every protocol
  message.  `yt2_party_block_parse` / `yt2_party_block_write` are the only
  places that copy file bytes into a struct (`memcpy`), and they check the
  signature and size first.
- The byte at a party signature's offset 0 is **not** part of the string: a
  written save has `00` there, while the game's static DGROUP template (what an
  untouched `CURGAME` holds) has `'S'`, so the same slot reads
  `"SMITHWARE PARTY"` there and `"MITHWARE PARTY"` at offset 1 in a save.  The
  parser checks bytes `01h..0Fh`.

## Save file (`CURGAME`, `SAVGAMEn`)

The game's save routine (`swreg.asm sub_0F823`) rebuilds the party block from
DGROUP:944Fh and copies the other six blocks from the current `CURGAME`
unchanged.  The file is 77,509 bytes (`0x12EC5`), seven contiguous chunks:

| offset | size | records | name | contents |
|---|---|---|---|---|
| `00000` | `01388` | 1 x 5000 | party | one 500-byte header + nine 500-byte roster slots |
| `01388` | `03840` | 144 x 100 | objects | runtime object records |
| `04BC8` | `0AC20` | 16 x 2754 | levels | one block per dungeon level |
| `0F7E8` | `00284` | 1 x 644 | world-table | world descriptor table |
| `0FA6C` | `00260` | 1 x 608 | world-index | world index |
| `0FCCC` | `00139` | 1 x 313 | world-map | overhead map |
| `0FE05` | `030C0` | 1 x 12480 | cache | working cache |

The chunk table is exported as `yt2_save_chunks[]`; its constants
(`YT2_SAVE_*`) come from the sizes/offsets the game's file-struct initializers
use (`fn_18312`, `fn_1838A`, `fn_182F4`, `fn_1834F`, `fn_1836E`, `fn_183A4`).
The four world/cache blocks are stored as sized byte records until their
internal fields are needed; the party and object record sizes are part of the
format.

## Party block (`0x1388`)

The first 5000 bytes of the save are DGROUP:944Fh..963Ah.  The first 500 bytes
are the game-state header, the remaining nine 500-byte slots are the roster
(the party is the non-empty subset; `SAVGAME1` from the M5 gate has the active
CONAN in slot 0 and the four recruitable characters in slots 5..8).

### Header (500 bytes)

| offset | field | notes |
|---|---|---|
| `000` | `signature[16]` | byte 0 `00` (save) or `'S'` (static template), then `MITHWARE PARTY` + NUL |
| `088` | `game_speed` | 5 in the shipped state (`v_94D7`) |
| `096` | `facing` | `4000h` south |
| `098` | `x` | party tile coordinate (`v_94E7`) |
| `09A` | `y` | party tile coordinate (`v_94E9`) |
| `09C` | `field_9c` | `v_94EB` |
| `09E` | `field_9e` | `v_94ED` |
| `0A0` | `field_a0` | `v_94EF` |
| `0A2` | `scene` | scene id (`v_94F1`) |
| `0A4..1F3` | reserved | rest of the header snapshot |

Only the fields the port and the gates use are named; the reserved spans keep
the layout exact.  The DGROUP labels for the rest are game-state globals, not
a second structure.

### Roster slot (500 bytes)

| offset | field | notes |
|---|---|---|
| `000` | `name[14]` | ASCIIZ |
| `00E` | `class_id` | 1..8 |
| `010` | `sex` | 1 male, 2 female |
| `012` | `age` | |
| `014` | `level` | 0 for a newly created character |
| `016` | `field_16` | always 1 in the shipped roster |
| `018` | `field_18` | |
| `01A` | `field_1a` | byte |
| `01B` | `weight` | 37 in most shipped slots, 0 for one recruit |
| `01C` | `status` | condition/roster flags |
| `01E..03B` | reserved | |
| `03C` | `current` | 64-byte stat block, current values |
| `07C` | `maximum` | 64-byte stat block, displayed/maximum values |
| `0BC..117` | reserved | |
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
| `01A` | `carry_capacity` | | `036..03F` | reserved |

In the shipped roster the two copies differ only in `hit_points` (CONAN has 4
current / 13 maximum); the second copy is what the character sheet displays.

## `PICTURES.VGA`

Ten picture groups pack the whole file back to back.  A picture is raw 8-bit
pixels, `width * height` bytes with tightly packed rows (no stride padding).
The groups are the picture-cache classes; the cache slot counts depend on how
much EMS is available.

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

The file is 13 054 330 bytes (`00C7317A`).  Palettes are separate: the logo
and Webfoot intro pictures use their own palettes, the rest the main game
palette.  B2 exposes a group table and a pointer to each picture; the gate
pins the group boundaries and an FNV-1a checksum of every group.

## `WORLD.DAT` map prefix

The map database is a flat prefix at the start of `WORLD.DAT`; the sound and
music records follow it.  The B2 loader in `engine/port/include/yt2/world.h`
reads this prefix into typed tiles, map descriptors and map names.

| offset | block | layout | notes |
|---|---|---|---|
| `00000` | tiles | 115 200 × 4 | 20×6 maps of 40×24 tiles, row-major |
| `70800` | map descriptors | 120 × 6 | `suffix[4]`, `name_index` byte, `type` byte |
| `70AD0` | map names | 70 × 20 | fixed-width, space padded |
| `71048` | rest | | SFX records |
| `8270A` | palettes | 9 × 768 | 6-bit RGB DAC triples |
| `8420A` | rest | | world/music records |

A tile is `terrain`, `flags` (block/passthrough), `object` index and one
reserved byte.  Descriptor type 0 is a filler map, 1 a world map, 2 a dungeon.
The shipped file is 1 796 340 bytes; the first descriptor is filler, map 21 is
`YENDOR-PORT HOPE` and map 24 is `PORT HOPE` dungeon level 1.

The nine palettes are loaded with `fn_1821A` (offset table at DS:0CE6F,
record size `300h`): palette 0 is the title/attract palette (FNV-1a 64
`E3275EE3AE459FB5`), and the intro/story code fades only selected ranges of a
palette.  Palettes are stored as 6-bit DAC values, not 8-bit.

## Other records

| record | source | notes |
|---|---|---|
| `yt2_file_record` | `swreg.asm FILE_* EQUs`, DS:904Bh.. | DOS handle/transfer block and an ASCIIZ name tail |
| `yt2_picture_class` | DS:7834h, ten 16-byte entries | picture-cache class table |
| `yt2_music_header` / `yt2_music_patch` | `docs/sbfmdrv.md` 4.1/4.4 | `CTMF` music block and 16-byte FM patches |
| `yt2_sfx_entry` | DS:0CF51h | 80 dwords of `WORLD.DAT` SFX offsets |

## Checker

`yt2saveinfo SAVEGAME` prints all seven chunks, the party position and every
non-empty roster slot through the typed model.  `tests/c/saveload.sh` runs it
on the save the game wrote through its own disk panel, and
`tests/c/windows.sh` requires the Linux and Windows builds to print identical
output.
