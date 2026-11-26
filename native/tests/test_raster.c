/* The integer rasterizer against double-precision references. */
#include "present.h"
#include "raster.h"
#include "reciprocal.h"
#include "rows.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Canaries catch writes outside the framebuffer. */
static struct { uint32_t before[256]; TRFramebuffer fb; uint32_t after[256]; } guarded;
#define fb (guarded.fb)
static const TRTarget whole = {fb.pixels, 0, TR_HEIGHT};
static uint32_t state = 123456789;

static uint32_t next_value(void)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

static int between(int low, int high)
{
    return low + (int)(next_value() % (uint32_t)(high - low + 1));
}

static void clear(void) { memset(fb.pixels, 0, sizeof fb.pixels); }

static TRScreenVertex vertex(int x, int y, int depth)
{
    TRScreenVertex v = {x, y, (int32_t)(((uint64_t)1 << 36) / (unsigned)depth)};
    return v;
}

static double cross(TRScreenVertex a, TRScreenVertex b, double x, double y)
{
    return (double)(b.x - a.x) * (y - a.y) - (double)(b.y - a.y) * (x - a.x);
}

/* The pixels a set of corners can reach, clipped to the screen. */
typedef struct { int x0, x1, y0, y1; } Box;

static Box box_of(const TRScreenVertex *t, int count)
{
    Box box = {TR_WIDTH, 0, TR_HEIGHT, 0};
    for (int i = 0; i < count; i++) {
        int x = t[i].x >> 4, y = t[i].y >> 4;
        if (x - 1 < box.x0) box.x0 = x - 1;
        if (x + 2 > box.x1) box.x1 = x + 2;
        if (y - 1 < box.y0) box.y0 = y - 1;
        if (y + 2 > box.y1) box.y1 = y + 2;
    }
    if (box.x0 < 0) box.x0 = 0;
    if (box.y0 < 0) box.y0 = 0;
    if (box.x1 > TR_WIDTH) box.x1 = TR_WIDTH;
    if (box.y1 > TR_HEIGHT) box.y1 = TR_HEIGHT;
    return box;
}

/* The smallest distance in pixels from a pixel centre to the triangle's edge
 * lines, negative when the centre is outside. */
static double inside_by(const TRScreenVertex *t, int x, int y)
{
    double px = x * 16 + 8, py = y * 16 + 8;
    double sign = cross(t[0], t[1], t[2].x, t[2].y) < 0 ? -1 : 1;
    double least = 1e30;
    for (int i = 0; i < 3; i++) {
        TRScreenVertex a = t[i], b = t[(i + 1) % 3];
        double length = hypot(b.x - a.x, b.y - a.y);
        double d = sign * cross(a, b, px, py) / length / 16;
        if (d < least) least = d;
    }
    return least;
}

static void random_triangle(TRScreenVertex *t, int spread)
{
    int cx = between(-400, TR_WIDTH * 16 + 400), cy = between(-400, TR_HEIGHT * 16 + 400);
    for (int i = 0; i < 3; i++)
        t[i] = vertex(cx + between(-spread, spread), cy + between(-spread, spread),
            between(48, 6400));
}

static void check_reciprocal(void)
{
    for (int i = 0; i < 2000000; i++) {
        uint32_t d = i < 100000 ? (uint32_t)i + 1 : (next_value() >> (next_value() % 31)) | 1;
        d &= 0x7fffffffu;
        if (!d) continue;
        int shift;
        uint32_t r = tr_reciprocal(d, &shift);
        double estimate = r * ldexp(1.0, shift - 47);
        assert(fabs(estimate * d - 1.0) < 1.0 / 16384);
    }
}

static void check_coverage(void)
{
    static const int spreads[] = {20, 60, 300, 2000, 15000};
    TRMaterial flat = {.color = 0x1234};
    const TRTexCoord uv[3] = {{0, 0}, {0, 0}, {0, 0}};
    for (int round = 0; round < 6000; round++) {
        TRScreenVertex t[3];
        random_triangle(t, spreads[round % 5]);
        for (int i = 0; i < 3; i++) { /* stay within the documented limits */
            if (t[i].x > TR_COORD_LIMIT) t[i].x = TR_COORD_LIMIT;
            if (t[i].x < -TR_COORD_LIMIT) t[i].x = -TR_COORD_LIMIT;
            if (t[i].y > TR_COORD_LIMIT) t[i].y = TR_COORD_LIMIT;
            if (t[i].y < -TR_COORD_LIMIT) t[i].y = -TR_COORD_LIMIT;
        }
        clear();
        tr_raster_triangle(&whole, &t[0], &t[1], &t[2], uv, &flat);
        bool degenerate = cross(t[0], t[1], t[2].x, t[2].y) == 0;
        Box box = box_of(t, 3);
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
            bool drawn = fb.pixels[y * TR_WIDTH + x] != 0;
            bool near_it = x >= box.x0 && x < box.x1 && y >= box.y0 && y < box.y1;
            if (degenerate || !near_it) { assert(!drawn); continue; }
            double d = inside_by(t, x, y);
            if (d > 0.1) assert(drawn);
            if (d < -0.1) assert(!drawn);
        }
    }
}

/* Triangles fanned around a point must cover each pixel of their union once:
 * no cracks and no overlap along shared edges. */
static void check_watertight(void)
{
    static const int radii[] = {40, 150, 700, 5000};
    TRMaterial flat = {.color = 0x1234};
    const TRTexCoord uv[3] = {{0, 0}, {0, 0}, {0, 0}};
    static unsigned char count[TR_WIDTH * TR_HEIGHT];
    for (int round = 0; round < 4000; round++) {
        int radius = radii[round % 4], sides = between(3, 8);
        TRScreenVertex centre = vertex(between(200, TR_WIDTH * 16 - 200),
            between(200, TR_HEIGHT * 16 - 200), 500), rim[8];
        double start = between(0, 359) * 3.14159265358979 / 180;
        for (int i = 0; i < sides; i++) {
            double angle = start + 2 * 3.14159265358979 * i / sides;
            double reach = radius * (between(70, 100) / 100.0);
            rim[i] = vertex(centre.x + (int)(reach * cos(angle)),
                centre.y + (int)(reach * sin(angle)), 500);
        }
        memset(count, 0, sizeof count);
        Box box = box_of(rim, sides);
        clear();
        for (int i = 0; i < sides; i++) {
            tr_raster_triangle(&whole, &centre, &rim[i], &rim[(i + 1) % sides], uv, &flat);
            for (int y = box.y0; y < box.y1; y++) for (int x = box.x0; x < box.x1; x++) {
                int p = y * TR_WIDTH + x;
                count[p] += fb.pixels[p] != 0;
                fb.pixels[p] = 0;
            }
        }
        /* Nothing may have been drawn outside the fan's bounds. */
        for (int p = 0; p < TR_WIDTH * TR_HEIGHT; p++) assert(!fb.pixels[p]);
        for (int y = box.y0; y < box.y1; y++) for (int x = box.x0; x < box.x1; x++) {
            assert(count[y * TR_WIDTH + x] <= 1);
            bool well_inside = true;
            for (int i = 0; i < sides && well_inside; i++) {
                TRScreenVertex a = rim[i], b = rim[(i + 1) % sides];
                double d = cross(a, b, x * 16 + 8, y * 16 + 8);
                double toward = cross(a, b, centre.x, centre.y);
                double length = hypot(b.x - a.x, b.y - a.y);
                well_inside = length > 0 && d * (toward < 0 ? -1 : 1) / length / 16 > 0.1;
            }
            if (well_inside) assert(count[y * TR_WIDTH + x] == 1);
        }
    }
}

/* Each texel names itself, so a drawn pixel reveals the sample taken. */
static uint16_t named[TR_TEXTURE_SIZE * TR_TEXTURE_SIZE], mips[TR_MIP_PIXELS];

static int wrapped_error(double expected, int texel)
{
    double d = fmod(texel + 0.5 - expected, TR_TEXTURE_SIZE);
    if (d > TR_TEXTURE_SIZE / 2) d -= TR_TEXTURE_SIZE;
    if (d < -TR_TEXTURE_SIZE / 2) d += TR_TEXTURE_SIZE;
    return (int)fabs(d);
}

static void check_attributes(bool floor_surface)
{
    TRTexture texture = {{named, mips, mips + 64 * 64}};
    /* A level surface: reciprocal depth rises linearly down the screen. */
    static int32_t floor_depth[TR_HEIGHT];
    const double far_inverse = 1.0 / 5000, slope = (1.0 / 300 - far_inverse) / (TR_HEIGHT * 16);
    for (int row = 0; row < TR_HEIGHT; row++)
        floor_depth[row] = (int32_t)(16 / (far_inverse + slope * (row * 16 + 8)));
    long checked = 0;
    for (int round = 0; round < 4000; round++) {
        TRScreenVertex t[3];
        double depth[3];
        int cx = between(100, TR_WIDTH * 16 - 100), cy = between(300, TR_HEIGHT * 16 - 300);
        int spread = between(100, 1200);
        for (int i = 0; i < 3; i++) {
            int x = cx + between(-spread, spread), y = cy + between(-spread / 2, spread / 2);
            if (floor_surface) {
                if (y < 0) y = 0;
                if (y > TR_HEIGHT * 16) y = TR_HEIGHT * 16;
                depth[i] = 1 / (far_inverse + slope * y);
            } else depth[i] = between(200, 6000);
            t[i] = vertex(x, y, (int)(depth[i] + 0.5));
            depth[i] = pow(2, 36) / t[i].z;
        }
        double area = cross(t[0], t[1], t[2].x, t[2].y);
        if (fabs(area) < 16 * 16 * 40) continue; /* slivers amplify rounding */
        /* About half a texel per pixel, so the full texture level is used. */
        TRTexCoord uv[3];
        int base_u = between(-3000, 3000), base_v = between(-3000, 3000);
        for (int i = 0; i < 3; i++) {
            uv[i].u = (int16_t)(base_u + (t[i].x - t[0].x) / 2);
            uv[i].v = (int16_t)(base_v + (t[i].y - t[0].y) / 4);
        }
        TRMaterial material = {.texture = &texture,
            .floor_depth = floor_surface ? floor_depth : NULL};
        clear();
        tr_raster_triangle(&whole, &t[0], &t[1], &t[2], uv, &material);
        Box box = box_of(t, 3);
        for (int y = box.y0; y < box.y1; y++) for (int x = box.x0; x < box.x1; x++) {
            uint32_t pixel = fb.pixels[y * TR_WIDTH + x];
            if (!pixel || inside_by(t, x, y) < 0.1) continue;
            double weight[3], u = 0, v = 0, z = 0, over = 0;
            for (int i = 0; i < 3; i++)
                weight[i] = cross(t[(i + 1) % 3], t[(i + 2) % 3], x * 16 + 8, y * 16 + 8) / area;
            for (int i = 0; i < 3; i++) {
                double w = floor_surface ? weight[i] / depth[i] : weight[i];
                u += w * uv[i].u; v += w * uv[i].v; over += w;
                z += weight[i] * t[i].z;
            }
            u /= over; v /= over;
            int texel = tr_pixel_color(pixel);
            assert(wrapped_error(u / 16, texel & 127) <= (floor_surface ? 2 : 1));
            assert(wrapped_error(v / 16, texel >> 7) <= (floor_surface ? 2 : 1));
            assert(fabs(tr_pixel_depth(pixel) - z / 65536) <= 2);
            checked++;
        }
    }
    assert(checked > 100000);
}

static void check_culling_and_extremes(void)
{
    TRMaterial both = {.color = 7}, one = {.color = 7, .cull_back = true};
    const TRTexCoord uv[3] = {{-32768, -32768}, {32767, 32767}, {0, 32767}};
    TRScreenVertex a = vertex(160, 160, 100), b = vertex(1600, 160, 200),
        c = vertex(160, 1600, 300);
    static TRFramebuffer front;
    clear();
    tr_raster_triangle(&whole, &a, &b, &c, uv, &one);
    assert(tr_pixel_color(fb.pixels[30 * TR_WIDTH + 30]) == 7);
    front = fb; clear();
    tr_raster_triangle(&whole, &a, &c, &b, uv, &one);
    assert(!fb.pixels[30 * TR_WIDTH + 30]);
    tr_raster_triangle(&whole, &a, &c, &b, uv, &both);
    assert(!memcmp(&front, &fb, sizeof fb));
    /* At equal depth the first triangle stays; a nearer one replaces it. */
    TRMaterial other = {.color = 8};
    tr_raster_triangle(&whole, &a, &b, &c, uv, &other);
    assert(!memcmp(&front, &fb, sizeof fb));
    TRScreenVertex d = vertex(160, 160, 60), e = vertex(1600, 160, 60),
        f = vertex(160, 1600, 60);
    tr_raster_triangle(&whole, &d, &e, &f, uv, &other);
    assert(tr_pixel_color(fb.pixels[30 * TR_WIDTH + 30]) == 8);

    /* Wrapping texture coordinates, screen-spanning slivers and both depth
     * extremes must stay inside the framebuffer without undefined behaviour. */
    TRTexture texture = {{named, mips, mips + 64 * 64}};
    static int32_t floor_depth[TR_HEIGHT];
    for (int row = 0; row < TR_HEIGHT; row++) floor_depth[row] = between(48 * 16, 6400 * 16);
    const int limit = TR_COORD_LIMIT;
    for (int round = 0; round < 40000; round++) {
        TRScreenVertex t[3];
        TRTexCoord extreme[3];
        for (int i = 0; i < 3; i++) {
            t[i] = vertex(between(0, 1) ? between(-limit, limit)
                    : between(-50, TR_WIDTH * 16 + 50),
                between(0, 1) ? between(-limit, limit) : between(-50, TR_HEIGHT * 16 + 50),
                between(0, 1) ? 48 : between(48, 6400));
            extreme[i].u = (int16_t)next_value(); extreme[i].v = (int16_t)next_value();
        }
        TRMaterial material = {.texture = between(0, 4) ? &texture : NULL,
            .floor_depth = between(0, 1) ? floor_depth : NULL,
            .color = 9, .cull_back = between(0, 1)};
        tr_raster_triangle(&whole, &t[0], &t[1], &t[2], extreme, &material);
    }
    for (int i = 0; i < 256; i++) assert(!guarded.before[i] && !guarded.after[i]);
}

static void check_levels(void)
{
    static uint16_t base[TR_TEXTURE_SIZE * TR_TEXTURE_SIZE], storage[TR_MIP_PIXELS];
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++)
        base[y * 128 + x] = (x & 1) ? 0xffff : 0;
    TRTexture built;
    tr_texture_build(&built, base, storage);
    assert(built.level[0] == base);
    /* Averaging black and white columns gives mid grey on every smaller level. */
    assert(built.level[1][0] == 0x8410 && built.level[2][32 * 32 - 1] == 0x8410);

    static uint16_t one[128 * 128], two[64 * 64], three[32 * 32];
    for (int i = 0; i < 128 * 128; i++) one[i] = 1;
    for (int i = 0; i < 64 * 64; i++) two[i] = 2;
    for (int i = 0; i < 32 * 32; i++) three[i] = 3;
    TRTexture levels = {{one, two, three}};
    TRMaterial material = {.texture = &levels};
    TRScreenVertex a = vertex(160, 160, 100), b = vertex(1440, 160, 100),
        c = vertex(160, 1440, 100); /* 80 pixels on a side */
    static const int spans[] = {40, 80, 160, 320, 640, 2000}; /* texels */
    static const int expected[] = {1, 1, 1, 2, 3, 3};
    for (int i = 0; i < 6; i++) {
        TRTexCoord uv[3] = {{0, 0}, {(int16_t)(spans[i] * 16), 0},
            {0, (int16_t)(spans[i] * 16)}};
        clear();
        tr_raster_triangle(&whole, &a, &b, &c, uv, &material);
        assert(tr_pixel_color(fb.pixels[20 * TR_WIDTH + 20]) == expected[i]);
    }
    /* A few pixels take one sample from the smallest level. */
    TRScreenVertex d = vertex(160, 160, 100), e = vertex(192, 160, 100),
        f = vertex(160, 192, 100);
    TRTexCoord uv[3] = {{0, 0}, {16, 0}, {0, 16}};
    clear();
    tr_raster_triangle(&whole, &d, &e, &f, uv, &material);
    assert(tr_pixel_color(fb.pixels[10 * TR_WIDTH + 10]) == 3);
}

/* A triangle drawn strip by strip must leave exactly the pixels, and the
 * depths, that drawing it at once does. */
static void check_strips(void)
{
    TRTexture texture = {{named, mips, mips + 64 * 64}};
    static int32_t floor_depth[TR_HEIGHT];
    for (int row = 0; row < TR_HEIGHT; row++) floor_depth[row] = between(48 * 16, 6400 * 16);
    static TRFramebuffer parts;
    static struct { uint32_t before[256]; TRBand strip; uint32_t after[256]; } narrow;
    static const int heights[] = {2, 6, 10, TR_BAND_HEIGHT};
    for (int round = 0; round < 3000; round++) {
        TRScreenVertex t[3];
        random_triangle(t, round % 3 == 0 ? 5000 : round % 3 == 1 ? 600 : 60);
        TRTexCoord uv[3];
        for (int i = 0; i < 3; i++) {
            uv[i].u = (int16_t)between(-4000, 4000); uv[i].v = (int16_t)between(-4000, 4000);
        }
        TRMaterial material = {.texture = between(0, 3) ? &texture : NULL,
            .floor_depth = between(0, 1) ? floor_depth : NULL, .color = 5};
        clear();
        tr_raster_triangle(&whole, &t[0], &t[1], &t[2], uv, &material);
        int height = heights[round % 4];
        for (int top = 0; top < TR_HEIGHT; top += height) {
            int bottom = top + height > TR_HEIGHT ? TR_HEIGHT : top + height;
            TRTarget target = {narrow.strip.pixels, top, bottom};
            memset(narrow.strip.pixels, 0, sizeof narrow.strip.pixels);
            tr_raster_triangle(&target, &t[0], &t[1], &t[2], uv, &material);
            memcpy(parts.pixels + top * TR_WIDTH, narrow.strip.pixels,
                (size_t)(bottom - top) * TR_WIDTH * sizeof(uint32_t));
            /* Rows below a short last strip stay untouched. */
            for (int i = (bottom - top) * TR_WIDTH; i < TR_WIDTH * TR_BAND_HEIGHT; i++)
                assert(!narrow.strip.pixels[i]);
        }
        assert(!memcmp(parts.pixels, fb.pixels, sizeof fb.pixels));
    }
    for (int i = 0; i < 256; i++) assert(!narrow.before[i] && !narrow.after[i]);
}

static void check_fill_and_present(void)
{
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) fb.pixels[i] = 1;
    tr_fill(fb.pixels + 8, 3, 0xabcd);
    for (int i = 0; i < 64; i++) assert(fb.pixels[i] == (i >= 8 && i < 32 ? 0xabcdu : 1u));

    static uint16_t screen[TR_WIDTH * TR_HEIGHT];
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) fb.pixels[i] = next_value();
    memset(screen, 0x5a, sizeof screen);
    const int top = 6, rows = 10;
    tr_present_rows(fb.pixels, screen + top * TR_WIDTH, rows);
    for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
        uint16_t want = y >= top && y < top + rows ?
            tr_pixel_color(fb.pixels[(y - top) * TR_WIDTH + x]) : 0x5a5a;
        assert(screen[y * TR_WIDTH + x] == want);
    }
}

int main(void)
{
    for (int i = 0; i < TR_TEXTURE_SIZE * TR_TEXTURE_SIZE; i++) named[i] = (uint16_t)i;
    check_reciprocal();
    check_coverage();
    check_watertight();
    check_attributes(false);
    check_attributes(true);
    check_culling_and_extremes();
    check_levels();
    check_strips();
    check_fill_and_present();
    puts("Rasterizer checks passed");
    return 0;
}
