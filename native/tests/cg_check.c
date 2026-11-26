/* Add-in that compares the assembly loops (sh4.S) with their C references on
 * random inputs. It needs no game assets. Build it with
 * `cmake --build build-cg --target temple_check`, run TRCHECK.g3a and expect
 * three OK lines. */
#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdint.h>
#include "present.h"
#include "rows.h"

#define ROW_CALLS 6000
#define PRESENT_PASSES 4

static TRFramebuffer frame;
static uint16_t texture[TR_TEXTURE_SIZE * TR_TEXTURE_SIZE];
static int32_t floor_depth[TR_HEIGHT];
static uint32_t state;

static uint32_t next(void)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

static int32_t between(int32_t low, int32_t high)
{
    return low + (int32_t)(next() % (uint32_t)(high - low + 1));
}

/* An edge position, often exactly on or beside a pixel centre, where an
 * off-by-one in the rounding would show. */
static uint32_t edge_position(int32_t low, int32_t high)
{
    int32_t fraction = between(0, 3) ? between(0, 65535) : between(0x7ffe, 0x8002);
    return (uint32_t)(between(low, high) * 65536 + fraction);
}

static uint32_t mix(uint32_t hash, uint32_t value)
{
    return (hash ^ value) * 16777619u;
}

static void random_frame(void)
{
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        /* Few distinct depths, so that equal depths are common. */
        frame.pixels[i] = (next() % 0x60u) << 24 | (next() & 0xffffu);
}

static uint32_t run_rows(void (*draw)(TRRows *))
{
    state = 2463534242u;
    for (int i = 0; i < TR_TEXTURE_SIZE * TR_TEXTURE_SIZE; i++) texture[i] = (uint16_t)next();
    for (int i = 0; i < TR_HEIGHT; i++) floor_depth[i] = between(48 * 16, 6400 * 16);
    random_frame();
    uint32_t hash = 2166136261u;
    for (int call = 0; call < ROW_CALLS; call++) {
        int mode = between(0, 2), width_bits = 7 - between(0, 2);
        uint32_t count = (uint32_t)between(1, 30);
        int first = between(0, TR_HEIGHT - (int)count);
        uint32_t u_mask = (1u << width_bits) - 1;
        TRRows rows = {
            .line = &frame.pixels[first * TR_WIDTH], .rows = count,
            .left_x = edge_position(-60, 230),
            .left_step = (uint32_t)(between(0, 1) ? between(-0x60000, 0x60000)
                : between(-3, 3) * 65536),
            .right_x = edge_position(-30, 260),
            .right_step = (uint32_t)(between(0, 1) ? between(-0x60000, 0x60000)
                : between(-3, 3) * 65536),
            .z = (uint32_t)between(0x20000000, 0x30000000),
            .zx = (uint32_t)between(-0x100000, 0x100000),
            .zy = (uint32_t)between(-0x100000, 0x100000),
            .u = next(), .ux = next() >> 8, .uy = next() >> 8,
            .v = next(), .vx = next() >> 8, .vy = next() >> 8,
            .texels = mode ? texture : NULL,
            .v_mask = u_mask << width_bits, .u_mask = u_mask,
            .color = next() & 0xffffu,
            .floor_depth = mode == 2 ? &floor_depth[first] : NULL,
            .v_shift = (uint32_t)(12 - width_bits),
        };
        if (between(0, 3) == 0) { /* flat depth, equal to many pixels' */
            rows.z = (uint32_t)between(0, 0x5f) << 24 | (next() & 0xffffu);
            rows.zx = rows.zy = 0;
        }
        draw(&rows);
        /* The state left behind matters too: the next half continues from it. */
        hash = mix(hash, (uint32_t)(rows.line - frame.pixels));
        hash = mix(hash, rows.rows); hash = mix(hash, rows.left_x);
        hash = mix(hash, rows.right_x); hash = mix(hash, rows.z);
        hash = mix(hash, rows.u); hash = mix(hash, rows.v);
        hash = mix(hash, rows.floor_depth ? (uint32_t)(rows.floor_depth - floor_depth) : 0);
        if (call % 100 == 99) {
            for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) hash = mix(hash, frame.pixels[i]);
            random_frame();
        }
    }
    return hash;
}

static uint32_t run_present(void (*present)(const TRFramebuffer *, uint16_t *))
{
    state = 88675123u;
    uint32_t hash = 2166136261u;
    for (int pass = 0; pass < PRESENT_PASSES; pass++) {
        random_frame();
        /* A marked border shows any write outside the scaled image. */
        for (int i = 0; i < DWIDTH * DHEIGHT; i++) gint_vram[i] = (uint16_t)(i * 7 + pass);
        present(&frame, gint_vram);
        for (int i = 0; i < DWIDTH * DHEIGHT; i++) hash = mix(hash, gint_vram[i]);
    }
    return hash;
}

static uint32_t run_fill(void (*fill)(uint32_t *, uint32_t, uint32_t))
{
    state = 521288629u;
    random_frame();
    uint32_t hash = 2166136261u;
    for (int pass = 0; pass < 200; pass++) {
        int lines = between(1, 200);
        int first = between(0, TR_WIDTH * TR_HEIGHT / 8 - lines);
        fill(frame.pixels + first * 8, (uint32_t)lines, next());
        for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) hash = mix(hash, frame.pixels[i]);
    }
    return hash;
}

int main(void)
{
    uint32_t rows_c = run_rows(tr_rows_c), rows_asm = run_rows(tr_rows_sh);
    uint32_t present_c = run_present(tr_present_scale_c);
    uint32_t present_asm = run_present(tr_present_scale);
    uint32_t fill_c = run_fill(tr_fill_c), fill_asm = run_fill(tr_fill_sh);
    dclear(C_WHITE);
    dprint(10, 20, C_BLACK, "ROWS    %s  %08x %08x", rows_c == rows_asm ? "OK" : "FAIL",
        (unsigned)rows_c, (unsigned)rows_asm);
    dprint(10, 40, C_BLACK, "PRESENT %s  %08x %08x", present_c == present_asm ? "OK" : "FAIL",
        (unsigned)present_c, (unsigned)present_asm);
    dprint(10, 60, C_BLACK, "FILL    %s  %08x %08x", fill_c == fill_asm ? "OK" : "FAIL",
        (unsigned)fill_c, (unsigned)fill_asm);
    dtext(10, 90, C_BLACK, "EXIT to quit");
    dupdate();
    while (getkey().key != KEY_EXIT) {}
    return 0;
}
