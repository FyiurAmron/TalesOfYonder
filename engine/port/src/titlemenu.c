#include "yt2/titlemenu.h"

#include <string.h>

/* Idle fuse in wall milliseconds: vanilla burns 75 frame events of 55ms
   BIOS ticks. Counted in polls at tick_ms. */
#define TITLEMENU_IDLE_MS 12375u

/* The menu click rows (DGROUP:5AFAh): x0, x1, y0, y1, id. */
static const int menu_clicks[][5] = {
    {47, 273, 36, 59, 1},
    {47, 273, 63, 86, 2},
    {60, 255, 90, 113, 3},
    {112, 200, 117, 140, 4},
    {86, 231, 144, 167, 5},
};

static int menu_hit(int x, int y)
{
    unsigned int i;

    for (i = 0; i < sizeof(menu_clicks) / sizeof(menu_clicks[0]); i++) {
        if (x >= menu_clicks[i][0] && x <= menu_clicks[i][1] &&
            y >= menu_clicks[i][2] && y <= menu_clicks[i][3]) {
            return menu_clicks[i][4];
        }
    }
    return 0;
}

int yt2_titlemenu_init(yt2_titlemenu *menu, yt2_intro *intro, unsigned int tick_ms)
{
    if (menu == NULL || intro == NULL) {
        return YT2_STATUS_ERROR;
    }
    menu->intro = intro;
    menu->tick_ms = tick_ms;
    return YT2_STATUS_OK;
}

yt2_intro_transition yt2_titlemenu_boot(yt2_titlemenu *menu)
{
    yt2_intro *intro = menu->intro;

    if (!intro->flag_p) {
        if (!yt2_intro_logo(intro)) {
            return yt2_titlemenu_run(menu);
        }
        if (!yt2_intro_attract(intro)) {
            return yt2_titlemenu_run(menu);
        }
        yt2_intro_gate_star(intro, 20u);
    }
    return yt2_titlemenu_run(menu);
}

yt2_intro_transition yt2_titlemenu_run(yt2_titlemenu *menu)
{
    yt2_intro *intro = menu->intro;
    unsigned long idle_max;
    unsigned long idle = 0u;
    int music_on = 1;

    if (menu->tick_ms == 0u) {
        idle_max = 225u;
    } else {
        idle_max = TITLEMENU_IDLE_MS / menu->tick_ms;
        if (idle_max == 0u) {
            idle_max = 1u;
        }
    }
menu_top:
    yt2_intro_fade_out(intro);
    yt2_intro_load_palette(intro, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x00u, 2u, 1, 1, 0u, 0u, 0u,
                   0u, 0u);
    yt2_intro_flush_cursor(intro, 0u, 230, 180);
    yt2_intro_fade_in(intro);
    idle = 0u;
    for (;;) {
        yt2_intro_input input;
        uint8_t key;

        if (intro->stop) {
            return YT2_INTRO_TRANSITION_NONE;
        }
        memset(&input, 0, sizeof(input));
        input.mouse_x = -1;
        input.mouse_y = -1;
        if (intro->poll != NULL) {
            intro->poll(&input, intro->user);
        }
        if (input.left_pressed) {
            int hit = menu_hit(input.mouse_x, input.mouse_y);

            if (hit == 1) {
                return YT2_INTRO_TRANSITION_CREATE;
            }
            if (hit == 2) {
                return YT2_INTRO_TRANSITION_LOAD;
            }
            if (hit == 3) {
                return YT2_INTRO_TRANSITION_EMBARK;
            }
            if (hit == 4) {
                return YT2_INTRO_TRANSITION_RULES;
            }
            if (hit == 5) {
                goto instructions;
            }
        } else if (input.have_key) {
            key = input.key;
            if (key >= 'a' && key <= 'z') {
                key = (uint8_t)(key - 32u);
            }
            if (key == 'C') {
                return YT2_INTRO_TRANSITION_CREATE;
            }
            if (key == 'A') {
                return YT2_INTRO_TRANSITION_LOAD;
            }
            if (key == 'E') {
                return YT2_INTRO_TRANSITION_EMBARK;
            }
            if (key == 'R') {
                return YT2_INTRO_TRANSITION_RULES;
            }
            if (key == 'I') {
                goto instructions;
            }
            if (key == 0x11u) {
                yt2_intro_music_stop(intro);
                return YT2_INTRO_TRANSITION_QUIT;
            }
            if (key == 0x0Du) {
                if (music_on) {
                    yt2_intro_music_stop(intro);
                    music_on = 0;
                } else {
                    music_on = 1;
                    yt2_intro_music_service(intro);
                }
            } else if (key != 0x13u) {
                /* Unknown keys fall through to the idle count below. */
            }
        } else {
            /* No event: one logical tick for the script clock. */
            yt2_intro_wait_ticks(intro, 1u);
        }
        /* Time passes whether or not input arrived. */
        if (++idle >= idle_max) {
            yt2_intro_fade_out(intro);
            yt2_intro_music_stop(intro);
            yt2_intro_wait_ticks(intro, 20u);
            if (!yt2_intro_logo(intro)) {
                goto menu_top;
            }
            if (!yt2_intro_attract(intro)) {
                goto menu_top;
            }
            yt2_intro_gate_star(intro, 20u);
            goto menu_top;
        }
    }
instructions:
    yt2_intro_fade_out(intro);
    yt2_intro_attract(intro);
    goto menu_top;
}
