#include "test.h"

#include "yt2/game.h"

/*
 * Regression tests for the string instructions the emitted code leans on.
 * The shared step helper used to advance both pointers for every operation,
 * so a LODSB/STOSB sprite loop moved the destination twice per pixel and
 * smeared the intro's animated sprites across each row.
 */

#define TEST_SEG 0x3000u

int test_failures;

static uint8_t test_read8(uint16_t offset)
{
    return YT2_MEM8(TEST_SEG, offset);
}

static void test_write8(uint16_t offset, uint8_t value)
{
    YT2_MEM8(TEST_SEG, offset) = value;
}

static void test_strings(void)
{
    g_m.r.ds = TEST_SEG;
    g_m.r.es = TEST_SEG;

    /* LODS moves the source pointer only. */
    test_write8(0x0100u, 0x5Au);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    g_m.r.ax = 0;
    g_m.r.flags = 0;
    yt2_string_lods(&g_m, 1, TEST_SEG, 0);
    CHECK((g_m.r.ax & 0xFFu) == 0x5Au);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0200u);

    /* STOS moves the destination pointer only. */
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    g_m.r.ax = 0xA5u;
    yt2_string_stos(&g_m, 1, 0);
    CHECK(test_read8(0x0200u) == 0xA5u);
    CHECK(g_m.r.di == 0x0201u);
    CHECK(g_m.r.si == 0x0100u);

    /* MOVS moves both. */
    test_write8(0x0100u, 0x3Cu);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    yt2_string_movs(&g_m, 1, TEST_SEG, 0);
    CHECK(test_read8(0x0200u) == 0x3Cu);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0201u);

    /* SCAS compares against the destination and moves the destination only. */
    test_write8(0x0300u, 0x77u);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    g_m.r.ax = 0x77u;
    yt2_string_scas(&g_m, 1, 0);
    CHECK(g_m.r.di == 0x0301u);
    CHECK(g_m.r.si == 0x0100u);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));

    /* CMPS moves both and compares. */
    test_write8(0x0100u, 0x11u);
    test_write8(0x0300u, 0x11u);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    yt2_string_cmps(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0301u);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));

    /* The direction flag reverses the step. */
    g_m.r.flags = YT2_FLAG_DF;
    g_m.r.si = 0x0101u;
    g_m.r.di = 0x0201u;
    yt2_string_lods(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x0100u);
    CHECK(g_m.r.di == 0x0201u);
    yt2_string_stos(&g_m, 1, 0);
    CHECK(g_m.r.di == 0x0200u);
    CHECK(g_m.r.si == 0x0100u);
    yt2_string_movs(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x00FFu);
    CHECK(g_m.r.di == 0x01FFu);
    g_m.r.flags = 0;

    /* Word forms step by two. */
    test_write8(0x0400u, 0x34u);
    test_write8(0x0401u, 0x12u);
    g_m.r.si = 0x0400u;
    g_m.r.di = 0x0500u;
    yt2_string_lods(&g_m, 2, TEST_SEG, 0);
    CHECK(g_m.r.ax == 0x1234u);
    CHECK(g_m.r.si == 0x0402u);
    CHECK(g_m.r.di == 0x0500u);
    yt2_string_stos(&g_m, 2, 0);
    CHECK(test_read8(0x0500u) == 0x34u);
    CHECK(test_read8(0x0501u) == 0x12u);
    CHECK(g_m.r.di == 0x0502u);

    /* REP counts CX down and advances by the whole block. */
    g_m.r.si = 0x0400u;
    g_m.r.di = 0x0600u;
    g_m.r.cx = 3u;
    yt2_string_movs(&g_m, 1, TEST_SEG, 1);
    CHECK(g_m.r.cx == 0u);
    CHECK(g_m.r.si == 0x0403u);
    CHECK(g_m.r.di == 0x0603u);
    CHECK(test_read8(0x0600u) == 0x34u);

    /* OUTS moves the source only. */
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    g_m.r.dx = 0x0080u;
    yt2_string_outs(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0200u);
}

/*
 * A mouse callback is an interrupt: the driver restores the interrupted
 * program's complete register file when the handler returns.  A callback
 * delivered from a present hook used to leave its AX/BX/CX/DX event
 * parameters in the drawing code - BX (a sprite copy width) became the
 * button mask, 0, and the row loop underflowed into the next segment.
 * The handler early-outs without a driver, which is enough: it preserves
 * its entry registers, so the values after the call are the driver's
 * event arguments unless the wrapper restores the interrupted set.
 */
static void test_mouse_callback_preserves_registers(void)
{
    uint16_t handler_segment = (uint16_t)(YT2_LOAD_SEG + 0x13A0u);
    yt2_registers saved;

    g_m.mouse_handler_cs = handler_segment;
    g_m.mouse_handler_ip = 0x0002u;
    g_m.mouse_mask = 0x0001u;
    g_m.mouse_buttons = 0;
    g_m.r.ax = 0x1234u;
    g_m.r.bx = 0x008Cu;
    g_m.r.cx = 0x5678u;
    g_m.r.dx = 0x9ABCu;
    g_m.r.si = 0x1111u;
    g_m.r.di = 0x2222u;
    g_m.r.bp = 0x3333u;
    g_m.r.ds = 0x4444u;
    g_m.r.es = 0x5555u;
    g_m.r.flags = YT2_FLAG_DF;
    saved = g_m.r;

    yt2_mouse_abs(&g_m, 100u, 50u);

    CHECK(g_m.r.ax == saved.ax);
    CHECK(g_m.r.bx == saved.bx);
    CHECK(g_m.r.cx == saved.cx);
    CHECK(g_m.r.dx == saved.dx);
    CHECK(g_m.r.si == saved.si);
    CHECK(g_m.r.di == saved.di);
    CHECK(g_m.r.bp == saved.bp);
    CHECK(g_m.r.ds == saved.ds);
    CHECK(g_m.r.es == saved.es);
    CHECK(g_m.r.sp == saved.sp);
    CHECK(g_m.r.flags == saved.flags);
}

typedef struct test_input_source {
    const uint8_t *data;
    uint32_t length;
    uint32_t offset;
    int reads;
} test_input_source;

static int test_input_read(void *context, uint8_t *destination, uint32_t capacity,
                           uint32_t *read)
{
    test_input_source *source = (test_input_source *)context;
    uint32_t left = source->length - source->offset;

    source->reads++;
    if (left > capacity) {
        left = capacity;
    }
    memcpy(destination, source->data + source->offset, left);
    source->offset += left;
    *read = left;
    return YT2_OK;
}

static int test_input_write(void *context, const uint8_t *frame, uint32_t length)
{
    (void)context;
    (void)frame;
    (void)length;
    return YT2_OK;
}

static void test_count_button(void *context, uint8_t button, uint8_t down)
{
    int *count = (int *)context;

    (void)button;
    (void)down;
    (*count)++;
}

/*
 * Input may only be taken from the link where the INT 33h callback can run.
 * A present pass inside a nested guest callback (the mouse handler itself,
 * or the INT 1Ch ISR) used to consume a queued button edge and drop it,
 * which is what made a physical click disappear until the next one.  With
 * the guard the record stays in the transport until a safe pass.
 */
static void test_link_poll_defers_input_while_nested(void)
{
    uint8_t frame[YT2_FRAME_HEADER_SIZE + YT2_INPUT_MAX_PAYLOAD];
    test_input_source source;
    yt2_transport transport;
    yt2_service service;
    yt2_input_handlers handlers;
    yt2_service saved_service = g_m.service;
    yt2_transport saved_transport = g_m.transport;
    int saved_ready = g_m.link_ready;
    int saved_nested = g_m.nested;
    int buttons = 0;
    uint32_t payload;

    memset(frame, 0, sizeof(frame));
    yt2_put_le16(frame + 0u, YT2_PROTOCOL_MAGIC);
    frame[2] = YT2_PROTOCOL_VERSION;
    frame[3] = YT2_FAMILY_INPUT;
    yt2_put_le32(frame + 6u, 0u);
    payload = (uint32_t)yt2_payload_input_mouse_button(
        frame + YT2_FRAME_HEADER_SIZE, 0u, 1u);
    yt2_put_le32(frame + 10u, payload);

    memset(&source, 0, sizeof(source));
    source.data = frame;
    source.length = YT2_FRAME_HEADER_SIZE + payload;

    memset(&transport, 0, sizeof(transport));
    transport.context = &source;
    transport.write_frame = test_input_write;
    transport.read = test_input_read;

    memset(&handlers, 0, sizeof(handlers));
    handlers.mouse_button = test_count_button;
    yt2_service_init(&service, &transport, &handlers, &buttons);

    g_m.transport = transport;
    g_m.service = service;
    g_m.link_ready = 1;

    g_m.nested = 1;
    yt2_link_poll(&g_m);
    CHECK(source.reads == 0);
    CHECK(buttons == 0);

    g_m.nested = 0;
    yt2_link_poll(&g_m);
    CHECK(source.reads > 0);
    CHECK(buttons == 1);

    g_m.link_ready = saved_ready;
    g_m.nested = saved_nested;
    g_m.service = saved_service;
    g_m.transport = saved_transport;
}

int main(void)
{
    yt2_game_init(&g_m, NULL);
    if (g_m.exited) {
        fprintf(stderr, "yt2game-tests: cannot initialise the machine\n");
        return 1;
    }
    test_strings();
    test_mouse_callback_preserves_registers();
    test_link_poll_defers_input_while_nested();
    yt2_game_shutdown(&g_m);
    if (test_failures != 0) {
        fprintf(stderr, "yt2game-tests: %d failure(s)\n", test_failures);
        return 1;
    }
    printf("yt2game-tests: OK\n");
    return 0;
}
