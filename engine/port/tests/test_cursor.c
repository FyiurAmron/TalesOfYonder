#include "test.h"

#include <stdlib.h>

#include "yt2/catalog.h"
#include "yt2/classes.h"
#include "yt2/cluebook.h"
#include "yt2/cursor.h"
#include "yt2/item_detail.h"
#include "yt2/items.h"
#include "yt2/monster_detail.h"
#include "yt2/monsters.h"
#include "yt2/spell_detail.h"
#include "yt2/spells.h"
#include "yt2/walkthrough.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_cursor(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_surface surface;

    CHECK(yt2_cursor_draw(NULL, NULL, YT2_CURSOR_HAND, 0, 0) ==
          YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);

    CHECK(yt2_cursor_draw(&surface, NULL, YT2_CURSOR_HAND, 24, 170) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_cursor_draw(&surface, &pictures, 510u, 24, 170) ==
          YT2_STATUS_ERROR);

    /* B1 catalog framebuffer (tick 8050): composition + hand at (24,170). */
    CHECK(yt2_catalog_render(&surface, &pictures, &world, 67u) ==
          YT2_STATUS_OK);
    CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24, 170) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0xBC1E5BFF608CB41B));

    /* B1 clue-book framebuffer (tick 7718): page + hand at (24,170). */
    CHECK(yt2_cluebook_render(&surface, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24, 170) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x7F29E8DA4ED08B2F));

    /* B1 spell-list framebuffers: composition + hand at (24,170). */
    {
        yt2_spells spells;
        static const unsigned int ranks[] = { 0u, 14u, 98u, 104u };
        static const uint64_t frames[] = {
            UINT64_C(0xCFDBBECC9F9F16B5), UINT64_C(0x2429157AFD25AAC1),
            UINT64_C(0xEE0BBBD290210786), UINT64_C(0xC4B3A494D5A4A386)
        };
        unsigned int i;

        CHECK(yt2_spells_load(&spells, &world) == YT2_STATUS_OK);
        for (i = 0u; i < 4u; i++) {
            CHECK(yt2_spells_render(&surface, &pictures, &spells, ranks[i]) ==
                  YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 spell-detail framebuffers: composition + hand at (24,170). */
    {
        yt2_spells spells;
        static const unsigned int ranks[] = {
            0u, 3u, 31u, 60u, 85u, 101u, 104u
        };
        static const uint64_t frames[] = {
            UINT64_C(0x916BA640827AFA98), UINT64_C(0x16101408AEA3C3D6),
            UINT64_C(0x817BCFB27BB2CF16), UINT64_C(0xD4E745A58E607F9D),
            UINT64_C(0x1EABDEB91376BE7B), UINT64_C(0xE3E1C49EC22C0BEB),
            UINT64_C(0x53346FBF26D54917)
        };
        unsigned int i;

        CHECK(yt2_spells_load(&spells, &world) == YT2_STATUS_OK);
        for (i = 0u; i < 7u; i++) {
            CHECK(yt2_spell_detail_render(&surface, &pictures, &spells,
                                          ranks[i]) == YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
        CHECK(yt2_spell_detail_render_index(&surface, &pictures, &spells,
                                            86u, 1) == YT2_STATUS_OK);
        CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                              170) == YT2_STATUS_OK);
        CHECK(checksum(surface.pixels, 320u * 200u) ==
              UINT64_C(0x63CBF477FAFCF197));
    }

    /* B1 class-list framebuffers: composition + hand at (24,170). */
    {
        yt2_spells spells;
        yt2_classes classes;
        static const unsigned int ranks[] = { 0u, 5u };
        static const uint64_t frames[] = {
            UINT64_C(0x7AFA214601883F62), UINT64_C(0x55F1BAF7B6BD4662)
        };
        unsigned int i;

        CHECK(yt2_spells_load(&spells, &world) == YT2_STATUS_OK);
        CHECK(yt2_classes_load(&classes, &spells) == YT2_STATUS_OK);
        for (i = 0u; i < 2u; i++) {
            CHECK(yt2_classes_render_list(&surface, &pictures, &classes,
                                          ranks[i]) == YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 class-detail framebuffers: composition + hand at (24,170). */
    {
        yt2_spells spells;
        yt2_classes classes;
        static const unsigned int line[] = {
            0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u
        };
        static const unsigned int rank[] = {
            0u, 51u, 0u, 49u, 0u, 31u, 0u, 46u, 0u, 46u, 0u, 30u
        };
        static const uint64_t frames[] = {
            UINT64_C(0x76A5453A840960FC), UINT64_C(0x42B33D023F962D96),
            UINT64_C(0x0138C759C66C1856), UINT64_C(0x2564FE0E08998DAF),
            UINT64_C(0x63C1B75D46B91767), UINT64_C(0xE367835A282778DF),
            UINT64_C(0xCB42493F0DF415A0), UINT64_C(0x63005E54BE975F27),
            UINT64_C(0x72DDA2EAD6DF54DE), UINT64_C(0xC71821B1C30286E1),
            UINT64_C(0xA85D9A37F1537BA3), UINT64_C(0x9FB6858F0C9BEF02)
        };
        unsigned int i;

        CHECK(yt2_spells_load(&spells, &world) == YT2_STATUS_OK);
        CHECK(yt2_classes_load(&classes, &spells) == YT2_STATUS_OK);
        for (i = 0u; i < 12u; i++) {
            CHECK(yt2_classes_render_detail(&surface, &pictures, &classes,
                                            line[i], rank[i]) ==
                  YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 item-page framebuffers: composition + hand at (24,170). */
    {
        yt2_items items;
        static const unsigned int category[] = {
            0u, 0u, 2u, 3u, 4u, 4u, 5u, 7u, 0u
        };
        static const unsigned int rank[] = {
            0u, 8u, 0u, 27u, 0u, 3u, 9u, 15u, 0u
        };
        static const unsigned int plus[] = {
            0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 5u
        };
        static const uint64_t frames[] = {
            UINT64_C(0x91CE67DA7FF309E7), UINT64_C(0x2702830565968DAB),
            UINT64_C(0x868E9D8AD5553FBA), UINT64_C(0x5DF78A9B1F62D09E),
            UINT64_C(0xBECD5C255F487506), UINT64_C(0xE524C09F983C961B),
            UINT64_C(0x9057EEE9DC080E97), UINT64_C(0x82CAF34C3719226A),
            UINT64_C(0x00B80B509A0B64D7)
        };
        unsigned int i;

        CHECK(yt2_items_load(&items, &world) == YT2_STATUS_OK);
        for (i = 0u; i < 9u; i++) {
            unsigned int file;

            CHECK(yt2_items_member_file(&items, category[i], rank[i],
                                        &file) == YT2_STATUS_OK);
            CHECK(yt2_item_page_render(&surface, &pictures, &items, file,
                                       category[i],
                                       plus[i]) == YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 item-list framebuffers: composition + hand at (24,170). */
    {
        static const unsigned int ranks[] = { 0u, 7u };
        static const uint64_t frames[] = {
            UINT64_C(0x48BE7A2D0D32473B), UINT64_C(0x80A4723D56DB98BB)
        };
        unsigned int i;

        for (i = 0u; i < 2u; i++) {
            CHECK(yt2_items_render_list(&surface, &pictures, ranks[i]) ==
                  YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 item-detail framebuffers: composition + hand at (24,170). */
    {
        yt2_items items;
        static const unsigned int category[] = {
            0u, 0u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 7u, 7u
        };
        static const unsigned int rank[] = {
            0u, 40u, 0u, 51u, 0u, 42u, 0u, 12u, 0u, 26u, 0u, 23u
        };
        static const uint64_t frames[] = {
            UINT64_C(0xD6D71D6FC44F7FC6), UINT64_C(0xAF8113E12D8B99B4),
            UINT64_C(0xC1CD5FB06BD75613), UINT64_C(0x19F902BACBDA0EB7),
            UINT64_C(0x2316D6859EB01DE6), UINT64_C(0xC8AC931BA36485AC),
            UINT64_C(0x58872EA6860D5648), UINT64_C(0x33A6DB83E1DCB7C8),
            UINT64_C(0x4B3C63587F21DB3A), UINT64_C(0x7D44AFCA91856924),
            UINT64_C(0x79D758C1D972A566), UINT64_C(0x6CCE5BE1E98A356E)
        };
        unsigned int i;

        CHECK(yt2_items_load(&items, &world) == YT2_STATUS_OK);
        for (i = 0u; i < 12u; i++) {
            CHECK(yt2_items_render_detail(&surface, &pictures, &items,
                                          category[i], rank[i]) ==
                  YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 info-screen framebuffers: composition + hand at (24,170). */
    CHECK(yt2_items_render_attribute(&surface, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24, 170) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x573F73AA79168D2C));
    CHECK(yt2_items_render_transport(&surface, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24, 170) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0xA4B8E969246B245C));

    /* B1 monster-detail framebuffers: all 61 excursion states (base+8)
       plus the hand at (24,170). */
    {
        yt2_monsters monsters;
        static const unsigned int pics[] = {
            18u, 28u, 58u, 88u, 118u, 38u, 78u, 78u, 78u, 18u, 188u,
            48u, 98u, 98u, 38u, 88u, 108u, 108u, 98u, 78u, 58u, 88u,
            138u, 138u, 148u, 8u, 208u, 58u, 198u, 168u, 108u, 28u,
            118u, 128u, 148u, 108u, 108u, 28u, 178u, 48u, 118u, 178u,
            8u, 168u, 48u, 68u, 118u, 98u, 158u, 68u, 38u, 128u, 8u,
            118u, 88u, 58u, 8u, 48u, 8u, 28u, 118u
        };
        static const uint64_t frames[] = {
            UINT64_C(0xF6B7F3AC18A1FFC3), UINT64_C(0x08300B46FC43AFFA),
            UINT64_C(0x6B133BB03CC4CB04), UINT64_C(0x506F370CC7A6F9BE),
            UINT64_C(0x9847FCF59C82F7A5), UINT64_C(0xDFCAD364831E71D0),
            UINT64_C(0xF4517E2959E3D069), UINT64_C(0x49EE9C435D6AAEBD),
            UINT64_C(0xB556DEF09308BDA7), UINT64_C(0xEAEB8D1D772FEFE9),
            UINT64_C(0x106B991B60ACBF2A), UINT64_C(0x4EA68266727694C6),
            UINT64_C(0xC1E91079E08E7A3E), UINT64_C(0x2B698BF69BCE13EF),
            UINT64_C(0x781D23E77B3A8A13), UINT64_C(0x962DA00220069C59),
            UINT64_C(0xD617567BAE35696F), UINT64_C(0xC0D91DE9A9E00A08),
            UINT64_C(0x3622FC426029BB82), UINT64_C(0x627D4745B1517158),
            UINT64_C(0x08DE640BBD12D357), UINT64_C(0x0BD0EC5605C73F18),
            UINT64_C(0xEC56F33F8602283E), UINT64_C(0x94C2C964A67EF8F0),
            UINT64_C(0x83304B62CBBB0E8C), UINT64_C(0x7F100E048FAC3C15),
            UINT64_C(0xC1128BBF7812D60F), UINT64_C(0xED8869EC023C77E2),
            UINT64_C(0x6CC6FDEADD40C780), UINT64_C(0x7200BC5FCCAB0BEE),
            UINT64_C(0x99AF899D7F84AED6), UINT64_C(0x28A141BDEDCD9300),
            UINT64_C(0xCD441722E91DA386), UINT64_C(0x809390E54075A339),
            UINT64_C(0x510F7E0EB1017256), UINT64_C(0xD12B01121DC5E70F),
            UINT64_C(0x1C811E26E14B2168), UINT64_C(0x62E3AFFFBF3E78C3),
            UINT64_C(0x683DF693B722A79B), UINT64_C(0x4EC539646A3A6A57),
            UINT64_C(0xBA5B3A17C59AF8E9), UINT64_C(0x3607C949057A2F50),
            UINT64_C(0xD6CDA4495E7F4654), UINT64_C(0x0B57622C865DED13),
            UINT64_C(0xBE818BC98A009229), UINT64_C(0xDE1C9FBC0D34DB23),
            UINT64_C(0x732FA90F255F9892), UINT64_C(0xAA2AC182A554E931),
            UINT64_C(0xCA39FD84EBF822D8), UINT64_C(0x9EA875584EAD7B9D),
            UINT64_C(0xAF930B4F66D8FE69), UINT64_C(0x002FB19E07ACB564),
            UINT64_C(0xBAF07248EF3F5005), UINT64_C(0x1B99064C3175A8C9),
            UINT64_C(0x19E08FB95AFC5B7A), UINT64_C(0xB237F1E77B499E8E),
            UINT64_C(0xFA4B8E33E37962DB), UINT64_C(0x87B3BBBEF604093F),
            UINT64_C(0xBB8458404802A57B), UINT64_C(0xCD3D50AF5A36FC4C),
            UINT64_C(0xF8AE921D0638C1E1)
        };
        unsigned int i;

        CHECK(yt2_monsters_load(&monsters, &world) == YT2_STATUS_OK);
        for (i = 0u; i < 61u; i++) {
            CHECK(yt2_monster_detail_render(&surface, &pictures, &monsters,
                                            i, pics[i]) == YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    /* B1 walkthrough framebuffers: composition + hand at (24,170). */
    {
        yt2_walkthrough walk;
        static const unsigned int pages[] = { 0u, 1u, 14u, 19u, 30u };
        static const uint64_t frames[] = {
            UINT64_C(0x687B3AA93D1BEBEC), UINT64_C(0x82CDA8EC351D07E3),
            UINT64_C(0x80161B205CE19807), UINT64_C(0x21724493C02D833A),
            UINT64_C(0x5605EEBBEFD64001)
        };
        unsigned int i;

        CHECK(yt2_walkthrough_load(&walk, &world) == YT2_STATUS_OK);
        for (i = 0u; i < 5u; i++) {
            CHECK(yt2_walkthrough_render(&surface, &pictures, &walk,
                                         pages[i]) == YT2_STATUS_OK);
            CHECK(yt2_cursor_draw(&surface, &pictures, YT2_CURSOR_HAND, 24,
                                  170) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    yt2_surface_free(&surface);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
