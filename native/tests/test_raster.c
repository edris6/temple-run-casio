/* Exercise the private rasterizer with a fixed set of textured triangles. */
#include "../src/renderer.c"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static TRFramebuffer fb;
static uint16_t texture[128 * 128];
static uint32_t state = 123456789;

static uint32_t next_value(void)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

int main(void)
{
    for (int depth = 48; depth <= 6400; depth++) {
        uint32_t inverse = 268435456u / (unsigned)depth;
        assert(divide_cached(INT_MIN, depth, inverse) == INT_MIN / depth);
        assert(divide_cached(INT_MAX, depth, inverse) == INT_MAX / depth);
        for (int i = 0; i < 16; i++) {
            uint32_t bits = next_value();
            int numerator = bits <= INT_MAX ? (int)bits : -1 - (int)(UINT_MAX - bits);
            assert(divide_cached(numerator, depth, inverse) == numerator / depth);
        }
    }
    state = 123456789;
    for (int i = 0; i < 128 * 128; i++) texture[i] = (uint16_t)next_value();
    uint64_t hash = 1469598103934665603ULL;
    for (int frame = 0; frame < 200; frame++) {
        clear_frame(&fb);
        for (int i = 0; i < 20; i++) {
            Projected p[3];
            for (int j = 0; j < 3; j++) {
                p[j].x = (int)(next_value() % 800) - 300;
                p[j].y = (int)(next_value() % 600) - 200;
                p[j].inv_z = (int)(next_value() % 5500000) + 1;
                p[j].u = (int)(next_value() % 32000000) - 16000000;
                p[j].v = (int)(next_value() % 32000000) - 16000000;
            }
            triangle(&fb, p[0], p[1], p[2], 0x5544, texture, false);
        }
        for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) {
            hash ^= fb.pixels[i]; hash *= 1099511628211ULL;
            hash ^= fb.depth[i]; hash *= 1099511628211ULL;
        }
    }
    /* Captured from the original 64-bit rasterizer. */
    assert(hash == 0xa26f717ee40fed57ULL);

    /* Thin, screen-spanning triangles make off-triangle interpolants exceed
     * 32 bits even though the covered pixels have valid projected depths. */
    const Projected edges[][3] = {
        {{-2048,-2048,1,1677721600,-1677721600},
         {2048,2048,5592405,-1677721600,1677721600},
         {2047,2048,1,1677721600,1677721600}},
        {{-2048,2048,5592405,-1677721600,-1677721600},
         {2048,-2048,1,1677721600,1677721600},
         {2048,-2047,5592405,-1677721600,1677721600}},
        {{-2048,0,1,1677721600,0},
         {2048,1,5592405,-1677721600,1677721600},
         {2047,2,1,0,-1677721600}}
    };
    for (int i = 0; i < 128 * 128; i++) texture[i] = (uint16_t)(i * 37u);
    clear_frame(&fb);
    for (size_t i = 0; i < sizeof edges / sizeof edges[0]; i++)
        triangle(&fb, edges[i][0], edges[i][1], edges[i][2], 0, texture, false);
    hash = 1469598103934665603ULL;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) {
        hash ^= fb.pixels[i]; hash *= 1099511628211ULL;
        hash ^= fb.depth[i]; hash *= 1099511628211ULL;
    }
    assert(hash == 0x6e899c7667a07a73ULL);

    /* A floor receding into the distance must interpolate UVs by depth. */
    const Projected a = {10,10,100000,0,0};
    const Projected b = {110,10,100000,127 << 16,0};
    const Projected c = {10,90,200000,0,127 << 16};
    for (int i = 0; i < 128 * 128; i++) texture[i] = (uint16_t)i;
    clear_frame(&fb);
    triangle(&fb, a, b, c, 0, texture, true);
    int corrected_v = fb.pixels[50 * TR_WIDTH + 30] / 128;
    clear_frame(&fb);
    triangle(&fb, a, b, c, 0, texture, false);
    int affine_v = fb.pixels[50 * TR_WIDTH + 30] / 128;
    assert(corrected_v >= 83 && corrected_v <= 86);
    assert(affine_v >= 62 && affine_v <= 64);

    const int16_t floor_vertices[][3] = {{0,-144,0}, {16,-144,0}, {0,-144,16}, {0,-100,0}};
    TRMesh floor_mesh = {0};
    floor_mesh.fixed_vertices = floor_vertices;
    assert(is_floor_face(&floor_mesh, 1, 0, 1, 2));
    assert(!is_floor_face(&floor_mesh, 5, 0, 1, 2)); /* coin */
    assert(!is_floor_face(&floor_mesh, 1, 0, 1, 3)); /* wall */
    puts("Rasterizer regression checks passed");
    return 0;
}
