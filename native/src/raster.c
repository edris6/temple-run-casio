/* Triangle setup for the SH4AL-DSP, which has no divide instruction and no
 * FPU: divisions go through small reciprocal tables, so setup is mostly
 * multiplications and shifts. rows.h's loops then fill the pixels. */
#include "raster.h"
#include "reciprocal.h"
#include "rows.h"

#define R1(i) (0x80000000u / (32768u + (i) * 128u + 64u))
#define R4(i) R1(i), R1((i) + 1), R1((i) + 2), R1((i) + 3)
#define R16(i) R4(i), R4((i) + 4), R4((i) + 8), R4((i) + 12)
#define R64(i) R16(i), R16((i) + 16), R16((i) + 32), R16((i) + 48)
/* Not const: data in RAM is quicker to read than data in the add-in's ROM. */
uint32_t tr_reciprocal_table[256] = {R64(0), R64(64), R64(128), R64(192)};

/* The low word of n >> shift (12 <= shift <= 42). A gradient too steep for 32
 * bits belongs to a sliver far thinner than a pixel; treat it as flat. */
static inline int32_t gradient(int64_t n, int shift)
{
    int32_t high = (int32_t)(n >> 32);
    uint32_t low = (uint32_t)n;
    if (shift >= 32) return high >> (shift - 32);
    int32_t overflow = high >> (shift - 1);
    if (overflow != 0 && overflow != -1) return 0;
    return (int32_t)((low >> shift) | ((uint32_t)high << (32 - shift)));
}

/* Up to this many pixel centres in the bounding box, use one sample. */
#define TINY_PIXELS 4

/* 2^30 / dy for short edges, which are most of them. */
#define EDGE_TABLE 1024
#define E1(i) ((1 << 30) / ((i) + !(i)))
#define E4(i) E1(i), E1((i) + 1), E1((i) + 2), E1((i) + 3)
#define E16(i) E4(i), E4((i) + 4), E4((i) + 8), E4((i) + 12)
#define E64(i) E16(i), E16((i) + 16), E16((i) + 32), E16((i) + 48)
#define E256(i) E64(i), E64((i) + 64), E64((i) + 128), E64((i) + 192)
static int32_t edge_inverse[EDGE_TABLE] = {E256(0), E256(256), E256(512), E256(768)};

/* An edge's x position at the current row centre, in 16.16 pixels. The value
 * depends only on the two end points, so neighbouring triangles agree on every
 * row and leave neither gaps nor overlaps. */
typedef struct { uint32_t x, step; } Edge;

static inline Edge edge_at(const TRScreenVertex *top, const TRScreenVertex *bottom, int row)
{
    int32_t dx = bottom->x - top->x, dy = bottom->y - top->y, step;
    if (dy < EDGE_TABLE) {
        step = (int32_t)(((int64_t)dx * edge_inverse[dy]) >> 14);
    } else {
        int shift;
        uint32_t r = tr_reciprocal((uint32_t)dy, &shift);
        step = (dx * (int32_t)r) >> (31 - shift);
    }
    int32_t offset = (int32_t)(((int64_t)step * (row * 16 + 8 - top->y)) >> 4);
    Edge e = {(uint32_t)(top->x * 4096) + (uint32_t)offset, (uint32_t)step};
    return e;
}

void tr_raster_visible(const TRTarget *target, const TRScreenVertex *a,
    const TRScreenVertex *b, const TRScreenVertex *c, const TRTexCoord uv[3],
    const TRMaterial *material)
{
    /* The rows and columns of the screen whose pixel centres the bounding box
     * holds. The target's share of the rows is taken later, so that the choice
     * below does not depend on how the screen is split. */
    int first = (tr_min3(a->y, b->y, c->y) + 7) >> 4;
    int end = (tr_max3(a->y, b->y, c->y) + 7) >> 4;
    if (first < 0) first = 0;
    if (end > TR_HEIGHT) end = TR_HEIGHT;
    int columns = ((tr_max3(a->x, b->x, c->x) + 7) >> 4) -
        ((tr_min3(a->x, b->x, c->x) + 7) >> 4);

    int rows_on_screen = end - first;
    if (first < target->top) first = target->top;
    if (end > target->bottom) end = target->bottom;
    if (first >= end) return;

    int32_t abx = b->x - a->x, aby = b->y - a->y;
    int32_t acx = c->x - a->x, acy = c->y - a->y;
    int32_t area = abx * acy - acx * aby;
    const TRTexCoord *ta = &uv[0], *tb = &uv[1], *tc = &uv[2];
    if (area < 0) {
        const TRScreenVertex *swap = b; b = c; c = swap;
        const TRTexCoord *swap_uv = tb; tb = tc; tc = swap_uv;
        area = -area;
        int32_t t = abx; abx = acx; acx = t;
        t = aby; aby = acy; acy = t;
    }

    const TRTexture *texture = material->texture;
    const int32_t *floor_depth = material->floor_depth;
    uint16_t color = material->color;
    uint32_t zx = 0, zy = 0, ux = 0, uy = 0, vx = 0, vy = 0;
    uint32_t row_z, row_u = 0, row_v = 0;
    const uint16_t *texels = NULL;
    uint32_t v_shift = 0, u_mask = 0, v_mask = 0;

    if (rows_on_screen * columns <= TINY_PIXELS) {
        /* Not worth interpolating: take one sample near the middle. */
        row_z = (uint32_t)((a->z >> 2) + (b->z >> 2) + (c->z >> 1));
        if (texture) {
            const int size = TR_TEXTURE_SIZE >> (TR_TEXTURE_LEVELS - 1);
            int u = (ta->u + tb->u + 2 * tc->u) >> 2, v = (ta->v + tb->v + 2 * tc->v) >> 2;
            /* 2048 coordinate units span the smallest level's 32 texels. */
            color = texture->level[TR_TEXTURE_LEVELS - 1]
                [((v >> 6) & (size - 1)) * size + ((u >> 6) & (size - 1))];
        }
    } else {
        int shift;
        int32_t r = (int32_t)tr_reciprocal((uint32_t)area, &shift);
        shift = 43 - shift;
        int32_t xb = acy * r, xc = aby * r, yc = abx * r, yb = acx * r;
#define GRADIENTS(db, dc, per_x, per_y) \
        per_x = (uint32_t)gradient((int64_t)(db) * xb - (int64_t)(dc) * xc, shift); \
        per_y = (uint32_t)gradient((int64_t)(dc) * yc - (int64_t)(db) * yb, shift);

        /* Attributes are linear in screen space. Anchor them at the pixel
         * holding vertex a, then express each row from column zero (modulo
         * 2^32). */
        int anchor_x = a->x >> 4, anchor_y = a->y >> 4;
        int fraction_x = 8 - (a->x & 15), fraction_y = 8 - (a->y & 15);
#define ROW_START(value, per_x, per_y) \
        ((uint32_t)(value) \
         + (uint32_t)(((int64_t)(int32_t)per_x * fraction_x \
             + (int64_t)(int32_t)per_y * fraction_y) >> 4) \
         - per_x * (uint32_t)anchor_x + per_y * (uint32_t)(first - anchor_y))

        GRADIENTS(b->z - a->z, c->z - a->z, zx, zy)
        row_z = ROW_START(a->z, zx, zy);
        if (texture) {
            /* 16.16 texels of the largest level. */
            int32_t au = ta->u * 4096, av = ta->v * 4096;
            int32_t bu = tb->u * 4096, bv = tb->v * 4096;
            int32_t cu = tc->u * 4096, cv = tc->v * 4096;

            /* Use a smaller level when a pixel covers several texels. This
             * reduces both shimmering and cache misses in the distance. */
            int32_t u1 = (tb->u - ta->u) >> 2, v1 = (tb->v - ta->v) >> 2;
            int32_t u2 = (tc->u - ta->u) >> 2, v2 = (tc->v - ta->v) >> 2;
            int32_t texel_area = u1 * v2 - u2 * v1;
            if (texel_area < 0) texel_area = -texel_area;
            int level = 0;
            if (texel_area > area >> 2)
                level = texel_area > area ? 2 : 1;
            au >>= level; av >>= level; bu >>= level; bv >>= level;
            cu >>= level; cv >>= level;

            if (floor_depth) {
                /* u/depth and v/depth are linear on screen; scale by depth. */
#define OVER_DEPTH(t, vertex) t = (int32_t)(((int64_t)t * vertex->z) >> 28);
                OVER_DEPTH(au, a) OVER_DEPTH(av, a)
                OVER_DEPTH(bu, b) OVER_DEPTH(bv, b)
                OVER_DEPTH(cu, c) OVER_DEPTH(cv, c)
#undef OVER_DEPTH
            }
            GRADIENTS(bu - au, cu - au, ux, uy)
            GRADIENTS(bv - av, cv - av, vx, vy)
            row_u = ROW_START(au, ux, uy);
            row_v = ROW_START(av, vx, vy);
            int width_bits = 7 - level;
            if (floor_depth) {
                v_shift = (uint32_t)(12 - width_bits);
            } else {
                /* Wrapping is harmless: only bits inside the texture count. */
                row_v <<= width_bits; vx <<= width_bits; vy <<= width_bits;
            }
            texels = texture->level[level];
            u_mask = (1u << width_bits) - 1;
            v_mask = u_mask << width_bits;
        }
#undef GRADIENTS
#undef ROW_START
    }

    const TRScreenVertex *top = a, *middle = b, *bottom = c;
    bool long_edge_left = true;
#define SORT(p, q) if (q->y < p->y) { const TRScreenVertex *s = p; p = q; q = s; \
        long_edge_left = !long_edge_left; }
    SORT(top, middle)
    SORT(top, bottom)
    SORT(middle, bottom)
#undef SORT

    TRRows rows = {
        .line = &target->pixels[(first - target->top) * TR_WIDTH],
        .z = row_z, .zx = zx, .zy = zy,
        .u = row_u, .ux = ux, .uy = uy,
        .v = row_v, .vx = vx, .vy = vy,
        .texels = texels, .v_mask = v_mask, .u_mask = u_mask, .color = color,
        .floor_depth = floor_depth && texels ? floor_depth + first : NULL,
        .v_shift = v_shift,
    };
    Edge long_edge = edge_at(top, bottom, first);
    if (long_edge_left) { rows.left_x = long_edge.x; rows.left_step = long_edge.step; }
    else { rows.right_x = long_edge.x; rows.right_step = long_edge.step; }
    int middle_row = (middle->y + 7) >> 4;
    if (middle_row > end) middle_row = end;
    int row = first;
    for (int half = 0; half < 2; half++) {
        int limit = half ? end : middle_row;
        if (row >= limit) continue;
        Edge short_edge = half ? edge_at(middle, bottom, row) : edge_at(top, middle, row);
        if (long_edge_left) { rows.right_x = short_edge.x; rows.right_step = short_edge.step; }
        else { rows.left_x = short_edge.x; rows.left_step = short_edge.step; }
        rows.rows = (uint32_t)(limit - row);
        tr_rows(&rows);
        row = limit;
    }
}

static uint16_t average(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    unsigned r = ((a >> 11) + (b >> 11) + (c >> 11) + (d >> 11) + 2) / 4;
    unsigned g = (((a >> 5) & 63) + ((b >> 5) & 63) + ((c >> 5) & 63) + ((d >> 5) & 63) + 2) / 4;
    unsigned blue = ((a & 31) + (b & 31) + (c & 31) + (d & 31) + 2) / 4;
    return (uint16_t)(r << 11 | g << 5 | blue);
}

void tr_texture_build(TRTexture *texture, const uint16_t *base, uint16_t *storage)
{
    texture->level[0] = base;
    const uint16_t *source = base;
    for (int level = 1; level < TR_TEXTURE_LEVELS; level++) {
        int size = TR_TEXTURE_SIZE >> level;
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) {
            const uint16_t *top = &source[(y * 2) * size * 2 + x * 2];
            const uint16_t *bottom = top + size * 2;
            storage[y * size + x] = average(top[0], top[1], bottom[0], bottom[1]);
        }
        texture->level[level] = source = storage;
        storage += size * size;
    }
}
