#ifndef YT2_DATA_H
#define YT2_DATA_H

#include <stddef.h>
#include <stdint.h>

/*
 * Typed views of the original's data records.
 *
 * The game keeps these records in its flat 16-bit address space, so the
 * transliterated code still reads and writes raw bytes there.  This header is
 * the single structured description of the records for the hand-written
 * boundary, the tools and the tests; the offsets are pinned against the
 * annotated asm and the shipped files with _Static_assert.  Nothing here
 * changes the wire format or the guest bytes: a record can be copied with
 * memcpy between the guest memory and one of these structs.
 *
 * All fields are fixed-width little-endian values, as in the original.  The
 * guest is 16-bit, the host is 32/64-bit: pointers and lengths never appear
 * inside these records.
 */

/* DOS file struct at DS:9040h (FILE_* EQUs in swreg.asm).  The name is an
   ASCIIZ tail, so the four structs in DGROUP have different lengths. */
typedef struct yt2_file_record {
    uint16_t handle;            /* +00 0FFFFh = closed */
    uint16_t segment;           /* +02 transfer buffer */
    uint16_t offset;            /* +04 */
    uint16_t count;             /* +06 block size in bytes */
    uint16_t index;             /* +08 block index */
    uint16_t base_lo;           /* +0A 32-bit file offset, low word */
    uint16_t base_hi;           /* +0C 32-bit file offset, high word */
    char name[];                /* +0E ASCIIZ */
} yt2_file_record;

#define YT2_FILE_NAME_OFFSET 0x0Eu

/* Picture-cache class entry at DS:7834h (16 bytes each, ten classes). */
typedef struct yt2_picture_class {
    uint16_t segment;           /* +00 chunk-descriptor block */
    uint16_t slots;             /* +02 cached slots */
    uint16_t size;              /* +04 width * height */
    uint16_t reserved;          /* +06 */
    uint16_t width;             /* +08 */
    uint16_t height;            /* +0A */
    uint32_t offset;            /* +0C PICTURES.VGA file offset */
} yt2_picture_class;

/* WORLD.DAT music block header (docs/sbfmdrv.md 4.1). */
typedef struct yt2_music_header {
    char magic[4];              /* 00 "CTMF" */
    uint8_t format;             /* 04 01 */
    uint8_t version;            /* 05 01 */
    uint16_t instrument_offset; /* 06 28h */
    uint16_t stream_offset;     /* 08 28h + 16 * instruments */
    uint16_t field_0a;          /* 0A 30h */
    uint16_t tempo;             /* 0C ticks per second (60h = 96 Hz) */
    uint8_t reserved_0e[6];
    uint8_t channel_flags[16];  /* 14 per-channel program-change flags */
    uint8_t instruments;        /* 24 1..9 */
    uint8_t reserved_25;
    uint8_t field_26;           /* 26 78h */
    uint8_t reserved_27;
} yt2_music_header;

/* One 16-byte FM patch in a music block: 11 register bytes + zero tail. */
typedef struct yt2_music_patch {
    uint8_t registers[11];
    uint8_t reserved[5];
} yt2_music_patch;

/* One entry of the game's SFX table at DS:0CF51h (80 dwords): the file
   offset of a Creative Voice File record in WORLD.DAT. */
typedef struct yt2_sfx_entry {
    uint32_t offset;
} yt2_sfx_entry;

/*
 * Party state: the first 5000-byte block of CURGAME / SAVEGAMEn.
 *
 * It is one 500-byte header followed by nine 500-byte party slots, the whole
 * block copied from DGROUP:944Fh when a game is saved.  The fields named here
 * are the ones the port and the gates use; the reserved spans are the rest of
 * the original record and are kept so the layout stays exact.
 */
#define YT2_PARTY_SIGNATURE_SIZE 16u
#define YT2_PARTY_HEADER_SIZE 0x1F4u
#define YT2_PARTY_SLOT_SIZE 0x1F4u
#define YT2_PARTY_SLOTS 9u
#define YT2_PARTY_BLOCK_SIZE (YT2_PARTY_HEADER_SIZE + \
                              YT2_PARTY_SLOTS * YT2_PARTY_SLOT_SIZE)

typedef struct yt2_party_header {
    /* 000: byte 00h is 00 in written saves and 'S' in the game's static
       DGROUP template ("SMITHWARE PARTY"); 01h starts "MITHWARE PARTY\0". */
    uint8_t signature[YT2_PARTY_SIGNATURE_SIZE];
    uint8_t reserved_10[0x88 - 0x10];
    uint16_t game_speed;        /* 088 v_94D7 */
    uint8_t reserved_8a[0x96 - 0x8A];
    uint16_t facing;            /* 096 v_94E5 */
    uint16_t x;                 /* 098 v_94E7 */
    uint16_t y;                 /* 09A v_94E9 */
    uint16_t field_9c;          /* 09C v_94EB */
    uint16_t field_9e;          /* 09E v_94ED */
    uint16_t field_a0;          /* 0A0 v_94EF */
    uint16_t scene;             /* 0A2 v_94F1 */
    uint8_t reserved_a4[YT2_PARTY_HEADER_SIZE - 0xA4];
} yt2_party_header;

/* The 0x40-byte stat block appears twice per slot: the first copy carries the
   current state, the second one the displayed/maximum values (they differ only
   in hit points in the shipped roster).  The field order is the stat panel's
   name table at DS:540Ch (swreg.asm); the six attributes run 45..60 in the
   shipped roster (README.DOC).  Carry capacity is ten times strength, as
   fn_0AB61 computes it. */
typedef struct yt2_party_stats {
    uint16_t strength;          /* 00 */
    uint16_t dexterity;         /* 02 */
    uint16_t stamina;           /* 04 */
    uint16_t intelligence;      /* 06 */
    uint16_t wisdom;            /* 08 */
    uint16_t charisma;          /* 0A */
    uint16_t projectile_accuracy; /* 0C */
    uint16_t projectile_damage; /* 0E */
    uint16_t battle_accuracy;   /* 10 */
    uint16_t battle_damage;     /* 12 */
    uint16_t absorption;        /* 14 */
    uint16_t hit_points;        /* 16 current in copy 1, maximum in copy 2 */
    uint16_t magic_points;      /* 18 */
    uint16_t carry_capacity;    /* 1A 10 * strength */
    uint16_t survival;          /* 1C */
    uint16_t projectile_skill;  /* 1E */
    uint16_t slashing;          /* 20 */
    uint16_t bashing;           /* 22 */
    uint16_t polearm;           /* 24 */
    uint16_t casting;           /* 26 */
    uint16_t mapping;           /* 28 */
    uint16_t navigation;        /* 2A */
    uint16_t bartering;         /* 2C */
    uint16_t repair;            /* 2E */
    uint16_t thievery;          /* 30 */
    uint16_t linguistics;       /* 32 */
    uint16_t chemistry;         /* 34 */
    uint16_t reserved_36[5];    /* 36..3F */
} yt2_party_stats;

/* A 4-byte inventory/equipment entry: the item id and its count, charges or
   state word.  The pack at 11Ah holds eight; the six equipment slots follow at
   13Ah.  The shipped roster's first equipment slot is 021Eh (the bare-hand
   item) and the pack is empty. */
typedef struct yt2_party_item {
    uint16_t id;
    uint16_t count;
} yt2_party_item;

#define YT2_PARTY_PACK_ITEMS 8u
#define YT2_PARTY_EQUIPMENT_ITEMS 6u
#define YT2_PARTY_EFFECTS 5u
#define YT2_PARTY_CONTAINERS 3u
#define YT2_PARTY_CONTAINER_SIZE 0x26u

typedef struct yt2_party_slot {
    char name[14];              /* 000 ASCIIZ */
    uint16_t class_id;          /* 00E 1..9 */
    uint16_t sex;               /* 010 1 male, 2 female */
    uint16_t portrait_pic;      /* 012 group-7 picture (0x13 + index) */
    uint16_t portrait_index;    /* 014 0..17, even male, odd female */
    uint16_t level;             /* 016 1 for a newly created character */
    uint16_t field_18;          /* 018 */
    uint8_t field_1a;           /* 01A */
    uint8_t weight;             /* 01B 37 in most shipped slots, 0 in one */
    uint16_t status;            /* 01C condition/roster flags */
    uint8_t reserved_1e[0x3C - 0x1E];
    yt2_party_stats current;    /* 03C */
    yt2_party_stats maximum;    /* 07C */
    uint8_t reserved_bc[0x118 - 0xBC];
    uint16_t carried_weight;    /* 118 */
    yt2_party_item pack[YT2_PARTY_PACK_ITEMS]; /* 11A */
    yt2_party_item equipment[YT2_PARTY_EQUIPMENT_ITEMS]; /* 13A */
    uint16_t effects[YT2_PARTY_EFFECTS]; /* 152 */
    uint16_t flags;             /* 15C */
    uint8_t reserved_15e[0x17C - 0x15E];
    uint8_t container[YT2_PARTY_CONTAINERS][YT2_PARTY_CONTAINER_SIZE]; /* 17C */
    uint8_t reserved_1ee[YT2_PARTY_SLOT_SIZE - 0x1EE];
} yt2_party_slot;

typedef struct yt2_party_block {
    yt2_party_header header;
    yt2_party_slot slots[YT2_PARTY_SLOTS];
} yt2_party_block;

/*
 * The rest of the save file: CURGAME / SAVEGAMEn is a fixed sequence of
 * blocks.  The save routine rewrites the party block from DGROUP and copies
 * the other six blocks from the current CURGAME unchanged (swreg.asm
 * sub_0F823).  Sizes come from the game's own constants: 0x90 object records
 * of 0x64 bytes, sixteen 0xAC2-byte level blocks, then the four WORLD.DAT
 * working-set blocks.
 */
#define YT2_SAVE_PARTY_OFFSET 0x00000u
#define YT2_SAVE_PARTY_SIZE 0x1388u
#define YT2_SAVE_OBJECTS_OFFSET 0x01388u
#define YT2_SAVE_OBJECTS_SIZE 0x3840u
#define YT2_SAVE_OBJECTS_COUNT 0x90u
#define YT2_SAVE_OBJECT_SIZE 0x64u
#define YT2_SAVE_LEVELS_OFFSET 0x04BC8u
#define YT2_SAVE_LEVELS_SIZE 0xAC20u
#define YT2_SAVE_LEVELS_COUNT 0x10u
#define YT2_SAVE_LEVEL_SIZE 0xAC2u
#define YT2_SAVE_WORLD_TABLE_OFFSET 0x0F7E8u
#define YT2_SAVE_WORLD_TABLE_SIZE 0x284u
#define YT2_SAVE_WORLD_INDEX_OFFSET 0x0FA6Cu
#define YT2_SAVE_WORLD_INDEX_SIZE 0x260u
#define YT2_SAVE_WORLD_MAP_OFFSET 0x0FCCCu
#define YT2_SAVE_WORLD_MAP_SIZE 0x139u
#define YT2_SAVE_CACHE_OFFSET 0x0FE05u
#define YT2_SAVE_CACHE_SIZE 0x30C0u
#define YT2_SAVE_SIZE 0x12EC5u

/* Sized record wrappers for the blocks that are not decoded field by field
   yet; the byte layout is the original's and the sizes are part of the save
   format. */
typedef struct yt2_object_record {
    uint8_t data[YT2_SAVE_OBJECT_SIZE];
} yt2_object_record;

typedef struct yt2_level_record {
    uint8_t data[YT2_SAVE_LEVEL_SIZE];
} yt2_level_record;

typedef enum yt2_save_chunk_id {
    YT2_SAVE_CHUNK_PARTY = 0,
    YT2_SAVE_CHUNK_OBJECTS,
    YT2_SAVE_CHUNK_LEVELS,
    YT2_SAVE_CHUNK_WORLD_TABLE,
    YT2_SAVE_CHUNK_WORLD_INDEX,
    YT2_SAVE_CHUNK_WORLD_MAP,
    YT2_SAVE_CHUNK_CACHE,
    YT2_SAVE_CHUNK_COUNT
} yt2_save_chunk_id;

typedef struct yt2_save_chunk {
    uint32_t offset;
    uint32_t size;
    uint32_t count;             /* records in the block */
    uint32_t record;            /* bytes per record */
    const char *name;
} yt2_save_chunk;

extern const yt2_save_chunk yt2_save_chunks[YT2_SAVE_CHUNK_COUNT];

/* The signature proper starts at 01h; byte 00h is 00 in written saves and 'S'
   in the game's static DGROUP template. */
int yt2_party_signature_check(const uint8_t *signature);

/* Copy a party block out of / into a save file.  The records are little-endian
   fixed-width fields with no padding (the asserts below pin that), so these
   are the only places the hand-written code treats file bytes as a struct. */
int yt2_party_block_parse(const uint8_t *source, size_t size,
                          yt2_party_block *party);
int yt2_party_block_write(const yt2_party_block *party, uint8_t *destination,
                          size_t capacity);

_Static_assert(offsetof(yt2_file_record, name) == YT2_FILE_NAME_OFFSET,
               "DOS file struct name offset");
_Static_assert(sizeof(yt2_picture_class) == 16, "picture class entry size");
_Static_assert(sizeof(yt2_music_header) == 0x28, "music header size");
_Static_assert(sizeof(yt2_music_patch) == 16, "music patch size");
_Static_assert(sizeof(yt2_sfx_entry) == 4, "SFX table entry size");
_Static_assert(offsetof(yt2_party_header, game_speed) == 0x88,
               "party header game speed offset");
_Static_assert(offsetof(yt2_party_header, facing) == 0x96,
               "party header facing offset");
_Static_assert(offsetof(yt2_party_header, x) == 0x98,
               "party header x offset");
_Static_assert(offsetof(yt2_party_header, y) == 0x9A,
               "party header y offset");
_Static_assert(offsetof(yt2_party_header, scene) == 0xA2,
               "party header scene offset");
_Static_assert(sizeof(yt2_party_header) == YT2_PARTY_HEADER_SIZE,
               "party header size");
_Static_assert(sizeof(yt2_party_stats) == 0x40, "party stat block size");
_Static_assert(offsetof(yt2_party_stats, carry_capacity) == 0x1A,
               "party stat carry capacity offset");
_Static_assert(offsetof(yt2_party_slot, weight) == 0x1B,
               "party slot weight offset");
_Static_assert(offsetof(yt2_party_slot, current) == 0x3C,
               "party slot current stats offset");
_Static_assert(offsetof(yt2_party_slot, maximum) == 0x7C,
               "party slot maximum stats offset");
_Static_assert(offsetof(yt2_party_slot, carried_weight) == 0x118,
               "party slot carried weight offset");
_Static_assert(sizeof(yt2_party_item) == 4, "party item size");
_Static_assert(offsetof(yt2_party_slot, pack) == 0x11A,
               "party slot pack offset");
_Static_assert(offsetof(yt2_party_slot, equipment) == 0x13A,
               "party slot equipment offset");
_Static_assert(offsetof(yt2_party_slot, effects) == 0x152,
               "party slot effects offset");
_Static_assert(offsetof(yt2_party_slot, flags) == 0x15C,
               "party slot flags offset");
_Static_assert(offsetof(yt2_party_slot, container) == 0x17C,
               "party slot container offset");
_Static_assert(sizeof(yt2_party_slot) == YT2_PARTY_SLOT_SIZE,
               "party slot size");
_Static_assert(sizeof(yt2_party_block) == YT2_PARTY_BLOCK_SIZE,
               "party block size");
_Static_assert(YT2_PARTY_BLOCK_SIZE == 0x1388u, "party block on-disk size");
_Static_assert(sizeof(yt2_object_record) == YT2_SAVE_OBJECT_SIZE,
               "object record size");
_Static_assert(sizeof(yt2_level_record) == YT2_SAVE_LEVEL_SIZE,
               "level record size");
_Static_assert(YT2_SAVE_PARTY_OFFSET + YT2_SAVE_PARTY_SIZE ==
               YT2_SAVE_OBJECTS_OFFSET, "save party block");
_Static_assert(YT2_SAVE_OBJECTS_OFFSET + YT2_SAVE_OBJECTS_SIZE ==
               YT2_SAVE_LEVELS_OFFSET, "save object block");
_Static_assert(YT2_SAVE_LEVELS_OFFSET + YT2_SAVE_LEVELS_SIZE ==
               YT2_SAVE_WORLD_TABLE_OFFSET, "save level block");
_Static_assert(YT2_SAVE_WORLD_TABLE_OFFSET + YT2_SAVE_WORLD_TABLE_SIZE ==
               YT2_SAVE_WORLD_INDEX_OFFSET, "save world table block");
_Static_assert(YT2_SAVE_WORLD_INDEX_OFFSET + YT2_SAVE_WORLD_INDEX_SIZE ==
               YT2_SAVE_WORLD_MAP_OFFSET, "save world index block");
_Static_assert(YT2_SAVE_WORLD_MAP_OFFSET + YT2_SAVE_WORLD_MAP_SIZE ==
               YT2_SAVE_CACHE_OFFSET, "save world map block");
_Static_assert(YT2_SAVE_CACHE_OFFSET + YT2_SAVE_CACHE_SIZE == YT2_SAVE_SIZE,
               "save cache block");

#endif /* YT2_DATA_H */
