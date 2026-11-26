/* Portable form of the loops in sh4.S, used on the host and as the reference
 * for tests/cg_check.c. Unused on the calculator. */
#include "raster.h"
#include "rows.h"

/* The low word of n >> shift (12 <= shift <= 43). A gradient too steep for 32
 * bits belongs to a sliver far thinner than a pixel; treat it as flat. */
static uint32_t gradient(int64_t n, int shift)
{
    int32_t high = (int32_t)(n >> 32);
    uint32_t low = (uint32_t)n;
    if (shift >= 32) return (uint32_t)(high >> (shift - 32));
    int32_t overflow = high >> (shift - 1);
    if (overflow != 0 && overflow != -1) return 0;
    return (low >> shift) | ((uint32_t)high << (32 - shift));
}

void tr_plane_c(const TRPlane *p, TRLinear *out, int32_t db, int32_t dc)
{
    uint32_t per_x = gradient((int64_t)db * p->xb - (int64_t)dc * p->xc, p->shift);
    uint32_t per_y = gradient((int64_t)dc * p->yc - (int64_t)db * p->yb, p->shift);
    /* Anchor at the pixel holding vertex a, then go back to column zero. */
    out->value += (uint32_t)(((int64_t)(int32_t)per_x * p->fraction_x
            + (int64_t)(int32_t)per_y * p->fraction_y) >> 4)
        - per_x * p->anchor_x + per_y * p->rows;
    out->dx = per_x;
    out->dy = per_y;
}

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
#define AT(type, field, offset) \
    _Static_assert(offsetof(type, field) == offset, "sh4.S: " #field)
AT(TRPlane, xb, 0); AT(TRPlane, xc, 4); AT(TRPlane, yc, 8); AT(TRPlane, yb, 12);
AT(TRPlane, shift, 16); AT(TRPlane, fraction_x, 20); AT(TRPlane, fraction_y, 24);
AT(TRPlane, anchor_x, 28); AT(TRPlane, rows, 32);
AT(TRRows, line, 0); AT(TRRows, rows, 4); AT(TRRows, left_x, 8);
AT(TRRows, left_step, 12); AT(TRRows, right_x, 16); AT(TRRows, right_step, 20);
AT(TRLinear, value, 0); AT(TRLinear, dx, 4); AT(TRLinear, dy, 8);
AT(TRRows, z, 24); AT(TRRows, u, 36); AT(TRRows, v, 48);
AT(TRRows, floor_depth, 60); AT(TRRows, texels, 64);
AT(TRRows, v_mask, 68); AT(TRRows, u_mask, 72); AT(TRRows, color, 76);
AT(TRRows, v_shift, 80);
_Static_assert(TR_WIDTH == 396, "sh4.S must match the frame width");
#endif

void tr_rows_c(TRRows *r)
{
    do {
        /* A pixel is covered when left edge <= centre < right edge. */
        int x0 = (int32_t)r->left_x >> 16;
        int x1 = (int32_t)r->right_x >> 16;
        if (x0 < 0) x0 = 0;
        if (x1 > TR_WIDTH) x1 = TR_WIDTH;
        if (x0 < x1) {
            uint32_t *p = r->line + x0, *end = r->line + x1;
            uint32_t z = r->z.value + r->z.dx * (uint32_t)x0;
            if (!r->texels) {
                do {
                    if (nearer(z, *p)) *p = (z & 0xffff0000u) | r->color;
                    z += r->z.dx;
                } while (++p != end);
            } else {
                uint32_t u = r->u.value + r->u.dx * (uint32_t)x0, du = r->u.dx;
                uint32_t v = r->v.value + r->v.dx * (uint32_t)x0, dv = r->v.dx;
                if (r->floor_depth) {
                    /* Depth is constant along a row of a level surface.
                     * Doubling turns texels into byte offsets. */
                    int32_t depth = *r->floor_depth;
                    u = times_depth(u, depth, 12) * 2;
                    du = times_depth(du, depth, 12) * 2;
                    v = times_depth(v, depth, r->v_shift) * 2;
                    dv = times_depth(dv, depth, r->v_shift) * 2;
                }
                do {
                    if (nearer(z, *p)) {
                        uint32_t offset = ((v >> 16) & r->v_mask) | ((u >> 16) & r->u_mask);
                        *p = (z & 0xffff0000u) | r->texels[offset / 2];
                    }
                    z += r->z.dx; u += du; v += dv;
                } while (++p != end);
            }
        }
        r->left_x += r->left_step; r->right_x += r->right_step;
        r->z.value += r->z.dy;
        r->line += TR_WIDTH;
        if (r->texels) {
            r->u.value += r->u.dy; r->v.value += r->v.dy;
            if (r->floor_depth) r->floor_depth++;
        }
    } while (--r->rows);
}

void tr_fill_c(uint32_t *pixels, uint32_t lines, uint32_t value)
{
    do {
        for (int i = 0; i < 8; i++) pixels[i] = value;
        pixels += 8;
    } while (--lines);
}
