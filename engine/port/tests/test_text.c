#include "test.h"

#include <string.h>

#include "yt2/text.h"

static const uint8_t glyph_a[YT2_TEXT_GLYPH_HEIGHT] = {
    0x20u, 0x50u, 0x88u, 0xF8u, 0x88u, 0x00u
};

static void check_glyph(const yt2_surface *surface, int origin_x, int origin_y,
                        uint8_t color)
{
    for (unsigned int row = 0; row < YT2_TEXT_GLYPH_HEIGHT; row++) {
        for (unsigned int column = 0; column < YT2_TEXT_GLYPH_WIDTH; column++) {
            int x = origin_x + (int)column;
            int y = origin_y + (int)row;
            uint8_t expected =
                ((glyph_a[row] << column) & 0x80u) != 0u ? color : 0xAAu;

            if (x >= 0 && (unsigned int)x < surface->width && y >= 0 &&
                (unsigned int)y < surface->height) {
                CHECK(surface->pixels[(size_t)y * surface->stride +
                                      (size_t)x] == expected);
            }
        }
    }
}

int test_text(void)
{
    yt2_surface surface;

    CHECK(yt2_text_width("") == 0u);
    CHECK(yt2_text_width("A") == YT2_TEXT_GLYPH_WIDTH);
    CHECK(yt2_text_width("CLUE BOOK") == 9u * YT2_TEXT_GLYPH_WIDTH);
    CHECK(yt2_text_width(NULL) == 0u);

    CHECK(yt2_surface_init(&surface, 16u, 10u) == YT2_STATUS_OK);
    yt2_surface_clear(&surface, 0xAAu);

    yt2_text_draw(&surface, 0, 0, "A", 0x55u);
    check_glyph(&surface, 0, 0, 0x55u);
    CHECK(surface.pixels[0] == 0xAAu);
    CHECK(surface.pixels[2] == 0x55u);
    CHECK(surface.pixels[6u * 16u + 15u] == 0xAAu);

    yt2_surface_clear(&surface, 0xAAu);
    yt2_text_draw(&surface, 14, 8, "A", 0x55u);
    CHECK(surface.pixels[8u * 16u + 14u] == 0xAAu);
    CHECK(surface.pixels[8u * 16u + 15u] == 0xAAu);
    CHECK(surface.pixels[9u * 16u + 14u] == 0xAAu);
    CHECK(surface.pixels[9u * 16u + 15u] == 0x55u);

    yt2_surface_clear(&surface, 0xAAu);
    yt2_text_draw(&surface, -2, -1, "A", 0x55u);
    CHECK(surface.pixels[0] == 0xAAu);
    CHECK(surface.pixels[1] == 0x55u);
    CHECK(surface.pixels[16] == 0xAAu);
    CHECK(surface.pixels[17] == 0xAAu);

    yt2_surface_clear(&surface, 0xAAu);
    yt2_text_draw(&surface, 0, 0, "\x01", 0x55u);
    for (size_t index = 0; index < 16u * 10u; index++) {
        CHECK(surface.pixels[index] == 0xAAu);
    }

    yt2_surface_clear(&surface, 0xAAu);
    yt2_text_draw(&surface, 0, 0, "AB", 0x55u);
    CHECK(surface.pixels[0] == 0xAAu);
    CHECK(surface.pixels[YT2_TEXT_GLYPH_WIDTH] != 0xAAu);

    yt2_surface_free(&surface);

    yt2_text_draw(NULL, 0, 0, "A", 0x55u);
    yt2_text_draw(&surface, 0, 0, "A", 0x55u);
    yt2_text_draw(&surface, 0, 0, NULL, 0x55u);
    return 0;
}
