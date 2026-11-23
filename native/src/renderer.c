/* Compact mesh renderer for the CG50. It is an initial platform
 * replacement, not a replacement for Unity's full rendering pipeline. */
#include "renderer.h"
#include <math.h>

typedef struct { float x, y, z; } Vec3;
typedef struct { int x, y; int32_t inv_z, u, v; } Projected;
typedef struct { Vec3 p; float u, v; } ClipVertex;
typedef struct { int kind; float forward, height, lateral, side; } Transform;

static int clampi(int x, int a, int b) { return x < a ? a : x > b ? b : x; }
static int edge(Projected a, Projected b, int x, int y)
{ return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x); }

static Vec3 world_vertex(const float *v, Transform t)
{
    Vec3 p;
    if ((t.kind >= 1 && t.kind <= 4) || t.kind == 6 || t.kind == 7) {
        /* Object81-Pivot's raw model coordinates include a large local pivot.
         * The corresponding prefab cancels it before rotating the mesh. */
        float origin_x = t.kind == 1 ? 559.9522f : t.kind == 6 ? 629.9522f :
            t.kind == 7 ? 689.9522f : t.kind == 3 ? 1349.952f : 1359.9521f;
        float origin_z = t.kind == 1 ? -1335.3497f : t.kind >= 6 ? -1275.3496f : t.kind == 2 ? -675.3496f :
            t.kind == 3 ? -575.3496f : -415.3496f;
        p.x = -(v[0] + origin_x) - t.lateral;
        p.y = v[2] + 30.0f - 15.0f - 9.0f;
        p.z = v[1] + origin_z + t.forward + 10.0f;
    } else if (t.kind == 0) {
        p.x = v[0];
        p.y = v[1] + 5.4f + t.height * 0.3f - 9.0f;
        p.z = v[2] + 20.0f + 10.0f;
    } else { /* coin prefab is not rotated with the wall meshes */
        p.x = v[0] + t.side - t.lateral;
        p.y = v[1] + 3.0f - 9.0f;
        p.z = v[2] + t.forward + 10.0f;
    }
    return p;
}

static int project(Vec3 p, Projected *out)
{
    if (p.z < 3.0f || p.z > 400.0f) return 0;
    out->x = clampi((int)(TR_WIDTH / 2 + p.x * 105.0f / p.z), -2048, 2048);
    out->y = clampi((int)(43 - p.y * 105.0f / p.z), -2048, 2048);
    out->inv_z = (int32_t)(16777216.0f / p.z);
    return 1;
}

static int64_t interpolated(Projected a, Projected b, Projected c,
    int x, int y, int area, int64_t va, int64_t vb, int64_t vc)
{
    return (edge(b,c,x,y)*va + edge(c,a,x,y)*vb + edge(a,b,x,y)*vc) / area;
}

static void triangle(TRFramebuffer *fb, Projected a, Projected b, Projected c,
    uint16_t color, const uint16_t *texture)
{
    int area = edge(a, b, c.x, c.y);
    if (!area) return;
    int left = clampi((int)fminf(a.x, fminf(b.x, c.x)), 0, TR_WIDTH - 1);
    int right = clampi((int)fmaxf(a.x, fmaxf(b.x, c.x)), 0, TR_WIDTH - 1);
    int top = clampi((int)fminf(a.y, fminf(b.y, c.y)), 0, TR_HEIGHT - 1);
    int bottom = clampi((int)fmaxf(a.y, fmaxf(b.y, c.y)), 0, TR_HEIGHT - 1);
    int64_t z0 = interpolated(a,b,c,left,top,area,a.inv_z,b.inv_z,c.inv_z);
    int64_t zx = interpolated(a,b,c,left+1,top,area,a.inv_z,b.inv_z,c.inv_z) - z0;
    int64_t zy = interpolated(a,b,c,left,top+1,area,a.inv_z,b.inv_z,c.inv_z) - z0;
    int64_t u0 = 0, v0 = 0, ux = 0, vx = 0, uy = 0, vy = 0;
    if (texture) {
        u0 = interpolated(a,b,c,left,top,area,a.u,b.u,c.u);
        v0 = interpolated(a,b,c,left,top,area,a.v,b.v,c.v);
        ux = interpolated(a,b,c,left+1,top,area,a.u,b.u,c.u) - u0;
        vx = interpolated(a,b,c,left+1,top,area,a.v,b.v,c.v) - v0;
        uy = interpolated(a,b,c,left,top+1,area,a.u,b.u,c.u) - u0;
        vy = interpolated(a,b,c,left,top+1,area,a.v,b.v,c.v) - v0;
    }
    int row0 = edge(a,b,left,top), row1 = edge(b,c,left,top), row2 = edge(c,a,left,top);
    for (int y = top; y <= bottom; y++) {
        int e0 = row0, e1 = row1, e2 = row2;
        int64_t z = z0, u = u0, v = v0;
        for (int x = left; x <= right; x++) {
            if ((area > 0 && e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                (area < 0 && e0 <= 0 && e1 <= 0 && e2 <= 0)) {
                int i = y * TR_WIDTH + x;
                uint16_t depth = (uint16_t)clampi((int)(z / 256), 1, 65535);
                if (depth > fb->depth[i]) {
                    fb->depth[i] = depth;
                    if (texture) {
                        uint32_t tu = (uint32_t)u, tv = (uint32_t)v;
                        fb->pixels[i] = texture[((tv >> 16) & 127u) * 128 + ((tu >> 16) & 127u)];
                    } else fb->pixels[i] = color;
                }
            }
            e0 += b.y-a.y; e1 += c.y-b.y; e2 += a.y-c.y;
            z += zx; u += ux; v += vx;
        }
        row0 -= b.x-a.x; row1 -= c.x-b.x; row2 -= a.x-c.x;
        z0 += zy; u0 += uy; v0 += vy;
    }
}

/* Clip in camera space before projection so triangles spanning the near
 * plane, especially the floor under the camera, do not disappear. */
static int clip_near(const ClipVertex in[3], ClipVertex out[4])
{
    int count = 0;
    for (int i = 0; i < 3; i++) {
        ClipVertex a = in[i], b = in[(i + 1) % 3];
        int a_in = a.p.z >= 3.0f, b_in = b.p.z >= 3.0f;
        if (a_in) out[count++] = a;
        if (a_in != b_in) {
            float f = (3.0f - a.p.z) / (b.p.z - a.p.z);
            ClipVertex v = {{a.p.x + (b.p.x-a.p.x)*f,
                             a.p.y + (b.p.y-a.p.y)*f, 3.0f},
                            a.u + (b.u-a.u)*f, a.v + (b.v-a.v)*f};
            out[count++] = v;
        }
    }
    return count;
}

static void mesh(TRFramebuffer *fb, const TRMesh *m, Transform t, uint16_t base)
{
    if (!m || !m->vertices || !m->indices) return;
    for (size_t i = 0; i + 2 < m->index_count; i += 3) {
        unsigned ia = m->indices[i], ib = m->indices[i+1], ic = m->indices[i+2];
        if (ia >= m->vertex_count || ib >= m->vertex_count || ic >= m->vertex_count) continue;
        unsigned ids[] = {ia,ib,ic};
        ClipVertex input[3], clipped[4];
        for (int k = 0; k < 3; k++) {
            input[k].p = world_vertex(m->vertices[ids[k]], t);
            input[k].u = m->uvs ? m->uvs[ids[k]][0] : 0;
            input[k].v = m->uvs ? m->uvs[ids[k]][1] : 0;
        }
        int count = clip_near(input, clipped);
        Projected p[4];
        int valid = 1;
        for (int k = 0; k < count; k++) {
            if (!project(clipped[k].p, &p[k])) { valid = 0; break; }
            float u = fmaxf(-200.0f, fminf(200.0f, clipped[k].u));
            float v = fmaxf(-200.0f, fminf(200.0f, clipped[k].v));
            p[k].u = (int32_t)(u * (128.0f * 65536.0f));
            p[k].v = (int32_t)((1.0f-v) * (128.0f * 65536.0f));
        }
        if (!valid) continue;
        for (int k = 1; k + 1 < count; k++)
            triangle(fb, p[0],p[k],p[k+1],
                (uint16_t)(base + ((i / 3) % 3) * 0x0821), m->uvs ? m->texture : NULL);
    }
}

static void render_frame(TRFramebuffer *fb, const TRCourse *course, const TRMesh *path,
    const TRMesh *player, const TRMesh *left_corner, const TRMesh *right_corner,
    float forward, float height, float lateral)
{
    for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
        int i = y * TR_WIDTH + x;
        fb->pixels[i] = y < 43 ? 0x8d9a : 0x3326;
        fb->depth[i] = 0;
    }
    float offset = fmodf(forward, 60.0f);
    int base_tile = (int)(forward / 60.0f);
    for (int i = -1; i < 5; i++) {
        float center = i * 60.0f - offset;
        if (center + 40.0f < 3.0f || center - 20.0f > 250.0f) continue;
        int tile = base_tile + i;
        if (course && tile > TR_OPENING_TILES - 1) continue;
        if (course && tile == TR_OPENING_TILES - 1) {
            int left = course->corner_direction < 0;
            Transform t = {left ? 6 : 7, center, 0, lateral, 0};
            mesh(fb, left ? left_corner : right_corner, t, 0x7be9);
        } else {
            Transform t = {1, center, 0, lateral, 0};
            mesh(fb, path, t, 0x7be9);
        }
    }
    Transform runner = {0, 0, height, lateral, 0};
    mesh(fb, player, runner, 0x9a63);
}

void tr_render_frame(TRFramebuffer *fb, const TRMesh *path, const TRMesh *player,
    float forward, float height, float lateral)
{
    render_frame(fb, NULL, path, player, NULL, NULL, forward, height, lateral);
}

void tr_render_opening(TRFramebuffer *fb, const TRCourse *course, const TRMesh *path,
    const TRMesh *player, const TRMesh *left_corner, const TRMesh *right_corner,
    float forward, float height, float lateral)
{
    render_frame(fb, course, path, player, left_corner, right_corner, forward, height, lateral);
}

void tr_render_course(TRFramebuffer *fb, const TRCourse *course, const TRMesh *jump,
    const TRMesh *slide, const TRMesh *trip, const TRMesh *coin,
    float forward, float lateral)
{
    for (int i = 0; i < TR_OPENING_TILES - 1; i++) {
        const TRTile *tile = &course->tiles[i];
        float center = i * 60.0f - forward;
        if (center < -30.0f || center > 260.0f) continue;
        if (tile->obstacle) {
            int kind = tile->obstacle == 1 ? 2 : tile->obstacle == 2 ? 3 : 4;
            const TRMesh *obstacle = tile->obstacle == 1 ? jump : tile->obstacle == 2 ? slide : trip;
            Transform t = {kind,center,0,lateral,0};
            mesh(fb, obstacle, t, 0x7be9);
        }
        if (tile->coin_lane != 2) for (int j = 0; j < 6; j++) {
            if (tile->coins_collected & (1u << j)) continue;
            float position = center - 24.0f + j * 9.0f;
            if (position < -10.0f || position > 220.0f) continue;
            Transform t = {5,position,0,lateral,6.0f * tile->coin_lane};
            mesh(fb, coin, t, 0xfc00);
        }
    }
}
