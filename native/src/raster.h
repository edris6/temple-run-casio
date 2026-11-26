#ifndef TEMPLE_RASTER_H
#define TEMPLE_RASTER_H

#include <stdbool.h>
#include <stdint.h>

#define TR_WIDTH 192
#define TR_HEIGHT 108
#define TR_TEXTURE_SIZE 128
#define TR_TEXTURE_LEVELS 3 /* 128, 64 and 32 texels square */
#define TR_MIP_PIXELS (64 * 64 + 32 * 32)

/* One word per pixel keeps the depth test and colour in the same cache line:
 * depth in the high half (larger is nearer, 0 is empty), RGB565 in the low. */
typedef struct {
    _Alignas(32) uint32_t pixels[TR_WIDTH * TR_HEIGHT]; /* on cache lines */
} TRFramebuffer;

static inline uint16_t tr_pixel_color(uint32_t pixel) { return (uint16_t)pixel; }
static inline uint16_t tr_pixel_depth(uint32_t pixel) { return (uint16_t)(pixel >> 16); }

typedef struct {
    const uint16_t *level[TR_TEXTURE_LEVELS];
} TRTexture;

/* x and y are in sixteenths of a pixel, with pixel centres at 8 mod 16, and
 * must stay within TR_COORD_LIMIT of the screen origin so that edge products
 * fit in 32 bits. z is 2^36 divided by the view depth in sixteenths of a world
 * unit (depth 48 or more). */
#define TR_COORD_LIMIT 16383
typedef struct {
    int32_t x, y;
    int32_t z;
} TRScreenVertex;

/* Texture coordinates in 1/2048 of the texture's width, v increasing downward.
 * Coordinates outside the texture repeat. */
typedef struct { int16_t u, v; } TRTexCoord;

/* Builds the smaller texture levels in storage (TR_MIP_PIXELS entries). */
void tr_texture_build(TRTexture *texture, const uint16_t *base, uint16_t *storage);

typedef struct {
    const TRTexture *texture;   /* without one, the triangle is filled with color */
    /* The view depth (1/256 world unit) of a level surface for each screen row.
     * When given, texture mapping is perspective-correct. */
    const int32_t *floor_depth;
    uint16_t color;
    bool cull_back;             /* skip triangles of negative screen area */
} TRMaterial;

/* Draws a triangle that tr_raster_triangle found may cover pixels. */
void tr_raster_visible(TRFramebuffer *fb, const TRScreenVertex *a,
    const TRScreenVertex *b, const TRScreenVertex *c, const TRTexCoord uv[3],
    const TRMaterial *material);

static inline int32_t tr_min3(int32_t a, int32_t b, int32_t c)
{
    if (b < a) a = b;
    return c < a ? c : a;
}

static inline int32_t tr_max3(int32_t a, int32_t b, int32_t c)
{
    if (b > a) a = b;
    return c > a ? c : a;
}

/* Draws one triangle with a depth test. uv gives the texture coordinates of a,
 * b and c, and is ignored without a texture. Triangles covering only a few
 * pixels are filled with one colour and depth. Most triangles of a distant or
 * closed mesh show nothing, so they are rejected here, inline. */
static inline void tr_raster_triangle(TRFramebuffer *fb, const TRScreenVertex *a,
    const TRScreenVertex *b, const TRScreenVertex *c, const TRTexCoord uv[3],
    const TRMaterial *material)
{
    /* Back faces, and triangles without area. */
    int32_t area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
    if (area <= 0 && (material->cull_back || !area)) return;
    /* No row of pixel centres, or none on the screen. */
    int32_t top = (tr_min3(a->y, b->y, c->y) + 7) >> 4;
    int32_t bottom = (tr_max3(a->y, b->y, c->y) + 7) >> 4;
    if (top == bottom || bottom <= 0 || top >= TR_HEIGHT) return;
    /* Likewise for columns. */
    int32_t left = (tr_min3(a->x, b->x, c->x) + 7) >> 4;
    int32_t right = (tr_max3(a->x, b->x, c->x) + 7) >> 4;
    if (left == right || right <= 0 || left >= TR_WIDTH) return;
    tr_raster_visible(fb, a, b, c, uv, material);
}

#endif
