#ifndef YT2_PAPERDOLL_H
#define YT2_PAPERDOLL_H

/*
 * The shared equipment-paperdoll verbs (B1 fn_1672C left-press, fn_16925
 * right-press, fn_0B3D2 recompute, the fn_16532 container-store
 * allocator): the in-game member panes and the creation items stage
 * run the SAME code over a small context, like the original, where
 * sub_148C4 calls fn_1672C/fn_16925 directly and only the take list
 * is creation-specific.
 *
 * The context borrows everything: the item tables, the save's
 * container-store region plus its bump/free allocator words (B1
 * v_95FD/v_95FF, global across creation and play), the caller's SFX
 * log, and a bonus hook (B1 fn_0ACF5/0B30D).  The cursor quad
 * (v_0CCA/v_40E2/v_40E6/v_40E4) is context-owned; wrappers sync it
 * with their own cursor fields around each call.
 *
 * Verbs paint nothing; they return what happened and the caller
 * repaints its own chrome (B1's fn_1658F pane repaint lands in the
 * caller too: the pack panes and the creation paperdoll paint
 * differently around the same cells).
 */

#include <stddef.h>
#include <stdint.h>

#include "yt2/items.h"
#include "yt2/party.h"
#include "yt2/status.h"

typedef struct yt2_paperdoll_ctx yt2_paperdoll_ctx;

/* Bonus hook (B1 fn_0ACF5 off / fn_0B30D on): combat applies the
   item's bonus list to the roster slot, creation no-ops (the fixed
   sub_01DFC list carries no bonus lists). */
typedef void (*yt2_paperdoll_bonus_fn)(void *bonus_ctx, unsigned int slot,
                                  unsigned int id, int plus);

/* Verb outcome: B1's v_50CC (0 changed, 1 miss, 2 examined). */
enum {
    YT2_PAPERDOLL_MISS = 0,
    YT2_PAPERDOLL_CHANGED = 1,
    YT2_PAPERDOLL_EXAMINED = 2
};

struct yt2_paperdoll_ctx {
    const yt2_items *items;
    uint8_t *store_base;          /* container-store region base */
    size_t store_size;            /* region bytes (index*0x22 must fit) */
    uint16_t *store_bump;         /* allocator bump counter */
    uint16_t *store_free;         /* allocator free-list head */
    uint16_t cursor;              /* v_0CCA: carried icon, 0 = arrow */
    uint16_t carried;             /* v_40E2: carried item id */
    uint16_t carried_count;       /* v_40E6: carried count/store */
    uint16_t carried_weight;      /* v_40E4: carried weight */
    unsigned int *sfx_log;        /* caller-owned SFX log array */
    unsigned int *sfx_count;      /* caller-owned SFX log length */
    unsigned int sfx_cap;         /* log capacity */
    yt2_paperdoll_bonus_fn bonus_apply;
    void *bonus_ctx;
};

/* One 0x60F4 paperdoll cell (pane-relative, inclusive bounds). */
typedef struct yt2_paperdoll_box {
    int x0;
    int x1;
    int y0;
    int y1;
    unsigned int id;
} yt2_paperdoll_box;

extern const yt2_paperdoll_box yt2_paperdoll_cells[21];

/* Bind a context (cursors zeroed, allocator untouched). */
void yt2_paperdoll_init(yt2_paperdoll_ctx *ctx, const yt2_items *items,
                   uint8_t *store_base, size_t store_size,
                   uint16_t *store_bump, uint16_t *store_free,
                   unsigned int *sfx_log, unsigned int *sfx_count,
                   unsigned int sfx_cap, yt2_paperdoll_bonus_fn bonus_apply,
                   void *bonus_ctx);

/* New parties reset the allocator (B1 v_95FD 2 / v_95FF 0). */
void yt2_paperdoll_new_party(yt2_paperdoll_ctx *ctx);

/* Pane-relative paperdoll-cell id at a point (B1 fn_0D1DE over 0x60F4). */
unsigned int yt2_paperdoll_hit(int ox, int oy, int x, int y);

/*
 * Left-press verb (B1 fn_1672C) on a paperdoll cell id: take from a full
 * slot, drop onto an empty one, swap with a full one, insert into
 * an occupied bag.  The slot is the character's 1-based roster
 * index for the bonus hook.  Returns YT2_PAPERDOLL_CHANGED or
 * YT2_PAPERDOLL_MISS; SFX 3 buzzes ride the log on refusals, SFX 6 on
 * placements.
 */
int yt2_paperdoll_left(yt2_paperdoll_ctx *ctx, yt2_character *character,
                  unsigned int slot, unsigned int id);

/*
 * Right-press verb (B1 fn_16925): cell 9 closes the staged pack bag,
 * bags open/toggle their staging, everything else is silent in
 * vanilla.  Returns YT2_PAPERDOLL_CHANGED (staging moved: repaint),
 * YT2_PAPERDOLL_EXAMINED (a readable/plain item was hit: B1 sets the
 * v_5110 context and the caller presents; *examined_id takes the
 * item id, 0 when none), or YT2_PAPERDOLL_MISS.
 */
int yt2_paperdoll_right(yt2_paperdoll_ctx *ctx, yt2_character *character,
                   unsigned int slot, unsigned int id,
                   unsigned int *examined_id);

/* fn_0B3D2: recompute combat currents/maxima from bases, skills,
   equipment rows and effect rows. */
void yt2_paperdoll_recompute(yt2_paperdoll_ctx *ctx, yt2_character *character);

/* sub_16E38: unequipping clears that slot's weapon wear counter. */
void yt2_paperdoll_wear_clear(yt2_character *character, unsigned int cell);

/* sub_16E04: flush a staging slot to its file store, then clear
   id+count (cells linger like B1).  Empty slots skip the write. */
void yt2_paperdoll_store_flush(yt2_paperdoll_ctx *ctx,
                           yt2_container_stage *stage);

#endif
