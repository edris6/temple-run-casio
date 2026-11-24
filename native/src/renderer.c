/* Compact mesh renderer for the CG50. It is an initial platform
 * replacement, not a replacement for Unity's full rendering pipeline. */
#include "renderer.h"
#include <math.h>

typedef struct { float x, y, z; } Vec3;
typedef struct { int x, y; int32_t inv_z, u, v; } Projected;
typedef struct { Vec3 p; float u, v; } ClipVertex;
typedef struct {
    int kind;
    float forward, height, lateral, side;
    bool world;
    float tile_x, tile_z, camera_x, camera_z;
    float view_x, view_z;
} Transform;

/* Most reconstructed meshes share vertices between triangles. Transform each
 * vertex once per instance instead of repeating costly software float work. */
#define VERTEX_CACHE_CAPACITY 1024
static ClipVertex vertex_cache[VERTEX_CACHE_CAPACITY];
static Projected projection_cache[VERTEX_CACHE_CAPACITY];
static unsigned char projection_valid[VERTEX_CACHE_CAPACITY];

static int dir_x(int d) { return d == 2 ? 1 : d == 4 ? -1 : 0; }
static int dir_z(int d) { return d == 1 ? 1 : d == 3 ? -1 : 0; }

static int clampi(int x, int a, int b) { return x < a ? a : x > b ? b : x; }
static int edge(Projected a, Projected b, int x, int y)
{ return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x); }

static Vec3 world_vertex(const float *v, Transform t)
{
    Vec3 p;
    if ((t.kind >= 1 && t.kind <= 4) || (t.kind >= 6 && t.kind <= 13)) {
        /* Object81-Pivot's raw model coordinates include a large local pivot.
         * The corresponding prefab cancels it before rotating the mesh. */
        float origin_x, origin_z;
        switch (t.kind) {
            case 1: origin_x = 559.9522f; origin_z = -1335.3497f; break;
            case 2: origin_x = 1359.9521f; origin_z = -675.3496f; break;
            case 3: origin_x = 1349.952f; origin_z = -575.3496f; break;
            case 4: origin_x = 1359.9521f; origin_z = -415.3496f; break;
            case 6: origin_x = 629.9522f; origin_z = -1275.3496f; break;
            case 7: origin_x = 689.9522f; origin_z = -1275.3496f; break;
            case 8: origin_x = 639.9522f; origin_z = -1335.3496f; break;
            case 9: origin_x = 699.9522f; origin_z = -1335.3496f; break;
            case 10: origin_x = 569.9522f; origin_z = -1405.3497f; break;
            case 11: origin_x = 1269.9521f; origin_z = -645.3496f; break;
            case 12: origin_x = 1269.952f; origin_z = -575.3496f; break;
            default: origin_x = 1269.952f; origin_z = -375.3496f; break;
        }
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
    if (t.world && t.kind != 0) {
        float x = p.x + t.tile_x - t.camera_x;
        float z = p.z - 10.0f + t.tile_z - t.camera_z;
        p.x = x * t.view_z - z * t.view_x;
        p.z = x * t.view_x + z * t.view_z + 30.0f;
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

static int project_vertex(ClipVertex vertex, Projected *out)
{
    if (!project(vertex.p, out)) return 0;
    float u = fmaxf(-200.0f, fminf(200.0f, vertex.u));
    float v = fmaxf(-200.0f, fminf(200.0f, vertex.v));
    out->u = (int32_t)(u * (128.0f * 65536.0f));
    out->v = (int32_t)((1.0f-v) * (128.0f * 65536.0f));
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
    int left = a.x < b.x ? a.x : b.x;
    int right = a.x > b.x ? a.x : b.x;
    int top = a.y < b.y ? a.y : b.y;
    int bottom = a.y > b.y ? a.y : b.y;
    if (c.x < left) left = c.x;
    if (c.x > right) right = c.x;
    if (c.y < top) top = c.y;
    if (c.y > bottom) bottom = c.y;
    /* Off-screen meshes are common when looking towards a corner. Avoid the
     * costly interpolation setup when no pixel can land in the framebuffer. */
    if (right < 0 || left >= TR_WIDTH || bottom < 0 || top >= TR_HEIGHT) return;
    left = clampi(left, 0, TR_WIDTH - 1);
    right = clampi(right, 0, TR_WIDTH - 1);
    top = clampi(top, 0, TR_HEIGHT - 1);
    bottom = clampi(bottom, 0, TR_HEIGHT - 1);
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
    bool cache = m->vertex_count <= VERTEX_CACHE_CAPACITY;
    if (cache) for (size_t i = 0; i < m->vertex_count; i++) {
        vertex_cache[i].p = world_vertex(m->vertices[i], t);
        vertex_cache[i].u = m->uvs ? m->uvs[i][0] : 0;
        vertex_cache[i].v = m->uvs ? m->uvs[i][1] : 0;
        projection_valid[i] = (unsigned char)project_vertex(vertex_cache[i], &projection_cache[i]);
    }
    for (size_t i = 0; i + 2 < m->index_count; i += 3) {
        unsigned ia = m->indices[i], ib = m->indices[i+1], ic = m->indices[i+2];
        if (ia >= m->vertex_count || ib >= m->vertex_count || ic >= m->vertex_count) continue;
        unsigned ids[] = {ia,ib,ic};
        if (cache && projection_valid[ia] && projection_valid[ib] && projection_valid[ic]) {
            triangle(fb, projection_cache[ia], projection_cache[ib], projection_cache[ic],
                (uint16_t)(base + ((i / 3) % 3) * 0x0821), m->uvs ? m->texture : NULL);
            continue;
        }
        ClipVertex input[3], clipped[4];
        for (int k = 0; k < 3; k++) {
            if (cache) input[k] = vertex_cache[ids[k]];
            else {
                input[k].p = world_vertex(m->vertices[ids[k]], t);
                input[k].u = m->uvs ? m->uvs[ids[k]][0] : 0;
                input[k].v = m->uvs ? m->uvs[ids[k]][1] : 0;
            }
        }
        int count = clip_near(input, clipped);
        Projected p[4];
        int valid = 1;
        for (int k = 0; k < count; k++) {
            if (!project_vertex(clipped[k], &p[k])) { valid = 0; break; }
        }
        if (!valid) continue;
        for (int k = 1; k + 1 < count; k++)
            triangle(fb, p[0],p[k],p[k+1],
                (uint16_t)(base + ((i / 3) % 3) * 0x0821), m->uvs ? m->texture : NULL);
    }
}

static void clear_frame(TRFramebuffer *fb)
{
    for (int y = 0; y < TR_HEIGHT; y++) for (int x = 0; x < TR_WIDTH; x++) {
        int i = y * TR_WIDTH + x;
        fb->pixels[i] = y < 43 ? 0x8d9a : 0x3326;
        fb->depth[i] = 0;
    }
}

void tr_render_frame(TRFramebuffer *fb, const TRMesh *path, const TRMesh *player,
    float forward, float height, float lateral)
{
    clear_frame(fb);
    float offset = fmodf(forward, 60.0f);
    for (int i = -1; i < 5; i++) {
        float center = i * 60.0f - offset;
        if (center + 40.0f < 3.0f || center - 20.0f > 250.0f) continue;
        Transform t = {.kind=1, .forward=center, .lateral=lateral};
        mesh(fb, path, t, 0x7be9);
    }
    Transform runner = {.kind=0, .height=height};
    mesh(fb, player, runner, 0x9a63);
}

static Transform world_transform(const TRCourse *course, int kind, float x, float z)
{
    Transform t = {0};
    t.kind = kind; t.world = true; t.tile_x = x; t.tile_z = z;
    t.camera_x = course->x; t.camera_z = course->z;
    t.view_x = course->view_x; t.view_z = course->view_z;
    if (t.view_x == 0 && t.view_z == 0) {
        t.view_x = (float)dir_x(course->heading); t.view_z = (float)dir_z(course->heading);
    }
    return t;
}

static int corner_index(const TRTile *tile)
{
    static const int left[] = {0,0,2,3,1};
    static const int right[] = {0,1,0,2,3};
    return tile->turn < 0 ? left[tile->incoming] : right[tile->incoming];
}

void tr_render_world(TRFramebuffer *fb, const TRCourse *course, const TRAssets *assets,
    float height)
{
    clear_frame(fb);
    for (int i = 0; i < course->count; i++) {
        const TRTile *tile = &course->tiles[i];
        float x = tile->x-course->x, z = tile->z-course->z;
        float vx = course->view_x, vz = course->view_z;
        if (vx == 0 && vz == 0) { vx = (float)dir_x(course->heading); vz = (float)dir_z(course->heading); }
        float ahead = x*vx+z*vz;
        float side = x*vz-z*vx;
        if (ahead < -70 || ahead > 280 || fabsf(side) > 150) continue;
        int axis = tile->incoming == 1 || tile->incoming == 3 ? 0 : 1;
        int kind = axis ? 10 : 1;
        const TRMesh *ground = assets->path[axis];
        if (tile->turn) {
            int corner = corner_index(tile);
            kind = 6 + corner;
            ground = assets->corner[corner];
        }
        Transform t = world_transform(course, kind, tile->x, tile->z);
        mesh(fb, ground, t, 0x7be9);
        if (tile->obstacle) {
            kind = (tile->obstacle == 1 ? 2 : tile->obstacle == 2 ? 3 : 4) + (axis ? 9 : 0);
            const TRMesh *obstacle = tile->obstacle == 1 ? assets->jump[axis] :
                tile->obstacle == 2 ? assets->slide[axis] : assets->trip[axis];
            t = world_transform(course, kind, tile->x, tile->z);
            mesh(fb, obstacle, t, 0x7be9);
        }
        if (tile->coin_lane != 2) for (int j = 0; j < 6; j++) {
            if (tile->coins_collected & (1u << j)) continue;
            float along = -24.0f + j * 9.0f, lane = 6.0f * tile->coin_lane;
            float coin_x = tile->x + along*dir_x(tile->incoming) + lane*dir_z(tile->incoming);
            float coin_z = tile->z + along*dir_z(tile->incoming) - lane*dir_x(tile->incoming);
            t = world_transform(course, 5, coin_x, coin_z);
            mesh(fb, assets->coin, t, 0xfc00);
        }
    }
    Transform runner = {.kind=0, .height=height};
    mesh(fb, assets->player, runner, 0x9a63);
}
