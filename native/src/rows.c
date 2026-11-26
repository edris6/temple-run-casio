/* Portable form of the loops in sh4.S, used on the host and as the reference
 * for tests/cg_check.c. Unused on the calculator. */
#include "raster.h"
#include "rows.h"

/* The depth halves are compared as signed 16-bit values; both stay positive. */
static int nearer(uint32_t z, uint32_t pixel)
{
    return (int16_t)(z >> 16) > (int16_t)(pixel >> 16);
}

static uint32_t times_depth(uint32_t value, int32_t depth, unsigned shift)
{
    return (uint32_t)((uint64_t)((int64_t)(int32_t)value * depth) >> shift);
}

#ifdef __sh__
#define AT(field, offset) _Static_assert(offsetof(TRRows, field) == offset, "sh4.S: " #field)
AT(line, 0); AT(rows, 4); AT(left_x, 8); AT(left_step, 12); AT(right_x, 16);
AT(right_step, 20); AT(z, 24); AT(zx, 28); AT(zy, 32); AT(u, 36); AT(ux, 40);
AT(uy, 44); AT(v, 48); AT(vx, 52); AT(vy, 56); AT(texels, 60); AT(v_mask, 64);
AT(u_mask, 68); AT(color, 72); AT(floor_depth, 76); AT(v_shift, 80);
_Static_assert(TR_WIDTH == 192, "sh4.S must match the frame width");
#endif

void tr_rows_c(TRRows *r)
{
    do {
        /* A pixel is covered when left edge <= centre < right edge. */
        int x0 = (int32_t)(r->left_x + 0x7fffu) >> 16;
        int x1 = (int32_t)(r->right_x + 0x7fffu) >> 16;
        if (x0 < 0) x0 = 0;
        if (x1 > TR_WIDTH) x1 = TR_WIDTH;
        if (x0 < x1) {
            uint32_t *p = r->line + x0, *end = r->line + x1;
            uint32_t z = r->z + r->zx * (uint32_t)x0;
            if (!r->texels) {
                do {
                    if (nearer(z, *p)) *p = (z & 0xffff0000u) | r->color;
                    z += r->zx;
                } while (++p != end);
            } else {
                uint32_t u = r->u + r->ux * (uint32_t)x0, du = r->ux;
                uint32_t v = r->v + r->vx * (uint32_t)x0, dv = r->vx;
                if (r->floor_depth) {
                    /* Depth is constant along a row of a level surface. */
                    int32_t depth = *r->floor_depth;
                    u = times_depth(u, depth, 12); du = times_depth(du, depth, 12);
                    v = times_depth(v, depth, r->v_shift);
                    dv = times_depth(dv, depth, r->v_shift);
                }
                do {
                    if (nearer(z, *p)) {
                        uint32_t index = ((v >> 16) & r->v_mask) | ((u >> 16) & r->u_mask);
                        *p = (z & 0xffff0000u) | r->texels[index];
                    }
                    z += r->zx; u += du; v += dv;
                } while (++p != end);
            }
        }
        r->left_x += r->left_step; r->right_x += r->right_step;
        r->z += r->zy; r->u += r->uy; r->v += r->vy;
        r->line += TR_WIDTH;
        if (r->floor_depth) r->floor_depth++;
    } while (--r->rows);
}

void tr_fill_c(uint32_t *pixels, uint32_t lines, uint32_t value)
{
    do {
        for (int i = 0; i < 8; i++) pixels[i] = value;
        pixels += 8;
    } while (--lines);
}
