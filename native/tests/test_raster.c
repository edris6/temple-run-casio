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
        tr_raster_triangle(&fb, &t[0], &t[1], &t[2], uv, &flat);
        bool degenerate = cross(t[0], t[1], t[2].x, t[2].y) == 0;
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
            bool drawn = fb.pixels[y * TR_WIDTH + x] != 0;
            if (degenerate) { assert(!drawn); continue; }
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
        TRScreenVertex centre = vertex(between(200, 2800), between(200, 1500), 500), rim[8];
        double start = between(0, 359) * 3.14159265358979 / 180;
        for (int i = 0; i < sides; i++) {
            double angle = start + 2 * 3.14159265358979 * i / sides;
            double reach = radius * (between(70, 100) / 100.0);
            rim[i] = vertex(centre.x + (int)(reach * cos(angle)),
                centre.y + (int)(reach * sin(angle)), 500);
        }
        memset(count, 0, sizeof count);
        for (int i = 0; i < sides; i++) {
            clear();
            tr_raster_triangle(&fb, &centre, &rim[i], &rim[(i + 1) % sides], uv, &flat);
            for (int p = 0; p < TR_WIDTH * TR_HEIGHT; p++) count[p] += fb.pixels[p] != 0;
        }
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
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
        tr_raster_triangle(&fb, &t[0], &t[1], &t[2], uv, &material);
        for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
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
    tr_raster_triangle(&fb, &a, &b, &c, uv, &one);
    assert(tr_pixel_color(fb.pixels[30 * TR_WIDTH + 30]) == 7);
    front = fb; clear();
    tr_raster_triangle(&fb, &a, &c, &b, uv, &one);
    assert(!fb.pixels[30 * TR_WIDTH + 30]);
    tr_raster_triangle(&fb, &a, &c, &b, uv, &both);
    assert(!memcmp(&front, &fb, sizeof fb));
    /* At equal depth the first triangle stays; a nearer one replaces it. */
    TRMaterial other = {.color = 8};
    tr_raster_triangle(&fb, &a, &b, &c, uv, &other);
    assert(!memcmp(&front, &fb, sizeof fb));
    TRScreenVertex d = vertex(160, 160, 60), e = vertex(1600, 160, 60),
        f = vertex(160, 1600, 60);
    tr_raster_triangle(&fb, &d, &e, &f, uv, &other);
    assert(tr_pixel_color(fb.pixels[30 * TR_WIDTH + 30]) == 8);

    /* Wrapping texture coordinates, screen-spanning slivers and both depth
     * extremes must stay inside the framebuffer without undefined behaviour. */
    TRTexture texture = {{named, mips, mips + 64 * 64}};
    static int32_t floor_depth[TR_HEIGHT];
    for (int row = 0; row < TR_HEIGHT; row++) floor_depth[row] = between(48 * 16, 6400 * 16);
    const int limit = TR_COORD_LIMIT;
    for (int round = 0; round < 200000; round++) {
        TRScreenVertex t[3];
        TRTexCoord extreme[3];
        for (int i = 0; i < 3; i++) {
            t[i] = vertex(between(0, 1) ? between(-limit, limit) : between(-50, 3100),
                between(0, 1) ? between(-limit, limit) : between(-50, 1800),
                between(0, 1) ? 48 : between(48, 6400));
            extreme[i].u = (int16_t)next_value(); extreme[i].v = (int16_t)next_value();
        }
        TRMaterial material = {.texture = between(0, 4) ? &texture : NULL,
            .floor_depth = between(0, 1) ? floor_depth : NULL,
            .color = 9, .cull_back = between(0, 1)};
        tr_raster_triangle(&fb, &t[0], &t[1], &t[2], extreme, &material);
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
        tr_raster_triangle(&fb, &a, &b, &c, uv, &material);
        assert(tr_pixel_color(fb.pixels[20 * TR_WIDTH + 20]) == expected[i]);
    }
    /* A few pixels take one sample from the smallest level. */
    TRScreenVertex d = vertex(160, 160, 100), e = vertex(192, 160, 100),
        f = vertex(160, 192, 100);
    TRTexCoord uv[3] = {{0, 0}, {16, 0}, {0, 16}};
    clear();
    tr_raster_triangle(&fb, &d, &e, &f, uv, &material);
    assert(tr_pixel_color(fb.pixels[10 * TR_WIDTH + 10]) == 3);
}

static void check_fill_and_present(void)
{
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) fb.pixels[i] = 1;
    tr_fill(fb.pixels + 8, 3, 0xabcd);
    for (int i = 0; i < 64; i++) assert(fb.pixels[i] == (i >= 8 && i < 32 ? 0xabcdu : 1u));

    static uint16_t screen[TR_SCREEN_WIDTH * TR_SCREEN_HEIGHT];
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) fb.pixels[i] = next_value();
    memset(screen, 0x5a, sizeof screen);
    tr_present_scale(&fb, screen);
    for (int y = 0; y < TR_SCREEN_HEIGHT; y++) for (int x = 0; x < TR_SCREEN_WIDTH; x++) {
        int sx = x - 6, sy = y - 4;
        uint16_t want = sx >= 0 && sx < TR_WIDTH * 2 && sy >= 0 && sy < TR_HEIGHT * 2 ?
            tr_pixel_color(fb.pixels[sy / 2 * TR_WIDTH + sx / 2]) : 0x5a5a;
        assert(screen[y * TR_SCREEN_WIDTH + x] == want);
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
    check_fill_and_present();
    puts("Rasterizer checks passed");
    return 0;
}
