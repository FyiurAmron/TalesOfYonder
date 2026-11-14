#ifndef YT2_TITLEMENU_H
#define YT2_TITLEMENU_H

#include "yt2/intro.h"

/*
 * The main menu (vanilla fn_0D36C): create/load/embark/rules/quit plus
 * idle-to-attract. Event-driven: the loop polls input and acts at once,
 * burning one logical tick per poll for the script clock and bridge
 * pacing. The idle fuse is wall-clock (~12.4s, vanilla's 75 frame events
 * of 55ms BIOS ticks), counted in polls at tick_ms. Clicks act on
 * capture (vanilla's 6-tick debounce delay is dropped: the coordinates
 * are already in hand, so the wait serves nothing).
 */

typedef struct yt2_titlemenu {
    yt2_intro *intro;      /* borrowed: surfaces, palette, poll, sequences */
    unsigned int tick_ms;  /* wall pacing per poll (0: unpaced, legacy fuse) */
} yt2_titlemenu;

int yt2_titlemenu_init(yt2_titlemenu *menu, yt2_intro *intro, unsigned int tick_ms);

/* The whole start-up flow (vanilla fn_01BE8): logo, attract, then the menu. */
yt2_intro_transition yt2_titlemenu_boot(yt2_titlemenu *menu);

/* Run the menu (vanilla fn_0D36C) to a transition. */
yt2_intro_transition yt2_titlemenu_run(yt2_titlemenu *menu);

#endif /* YT2_TITLEMENU_H */
