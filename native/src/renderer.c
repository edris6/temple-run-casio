/* Scene traversal and vertex stage for the CG50: integer transforms of the
 * imported meshes, culling and clipping. raster.c draws the triangles. This is
 * a compact platform replacement, not Unity's rendering pipeline. */
#include "renderer.h"
#include "reciprocal.h"
#include "rows.h"

/* View space is in sixteenths of a world unit; screen space in sixteenths of
 * a pixel. The camera looks slightly down from behind the runner. */
#define NEAR_Z 48
#define FAR_Z 6400
#define FOCAL_PIXELS 210
#define HORIZON_ROW 90 /* the screen row level with the camera */
#define FOCAL (FOCAL_PIXELS * 16)
#define CENTRE_X (TR_WIDTH / 2 * 16)
#define CENTRE_Y (HORIZON_ROW * 16)
#define SCREEN_RIGHT (TR_WIDTH * 16 - CENTRE_X)
#define SCREEN_BOTTOM (TR_HEIGHT * 16 - CENTRE_Y)
#define GUARD 13000 /* projected vertices stay this close to the centre */
#define PITCH_COS 3923 /* Q12 */
#define PITCH_SIN 1177
#define CAMERA_HEIGHT 208
#define CAMERA_BACK 480
#define FLOOR_Y (-144) /* prefab-local height of the temple floor */
#define CLIP_LIMIT 12
/* Lengths of the normals of the view's side, top and bottom planes, that is
 * hypot(FOCAL_PIXELS, distance to that screen edge), rounded up. */
#define SIDE_NORMAL 289
#define TOP_NORMAL 229
#define BOTTOM_NORMAL 250
_Static_assert(FOCAL_PIXELS == 210 && TR_WIDTH == 396 && TR_HEIGHT == 224 &&
    HORIZON_ROW == 90, "recompute the view plane normals");
_Static_assert(CENTRE_X + GUARD <= TR_COORD_LIMIT, "projection exceeds the rasterizer");
#define MATERIAL_LIMIT 32
#define SKY 0x8d9a
#define GROUND 0x3326

enum {
    OUT_LEFT = 1, OUT_RIGHT = 2, OUT_TOP = 4, OUT_BOTTOM = 8, OUT_NEAR = 16,
    OUT_FAR = 32, OUT_GUARD = 64,
    OUTSIDE = OUT_LEFT | OUT_RIGHT | OUT_TOP | OUT_BOTTOM | OUT_NEAR
};

typedef struct {
    TRScreenVertex screen; /* unset if near, far or guard */
    uint32_t flags;
} Vertex;

typedef struct {
    int32_t offset_x, offset_z; /* mesh origin relative to the camera */
    int32_t view_x, view_z;     /* camera facing, Q12 */
    int32_t height;             /* runner's jump */
    bool runner, sliding, floor;
    int junction_part, incoming;
} Instance;

typedef struct { int32_t x, y, z, u, v; } ClipVertex;

/* A triangle ready to draw. */
typedef struct {
    int16_t x[3], y[3];
    int32_t z[3];
    TRTexCoord uv[3];
    TRBounds bounds;
    uint8_t material;
} Listed;
_Static_assert(MATERIAL_LIMIT <= 256, "materials are listed by a byte");
_Static_assert((TR_HEIGHT + TR_BAND_HEIGHT - 1) / TR_BAND_HEIGHT <= 16,
    "each strip needs a bit in scene_bands");

/* The strip bits of two neighbouring triangles in the list. */
typedef union { uint32_t both; uint16_t one[2]; } Pair;

/* Each position is transformed once per instance. The view-space copy is only
 * needed for clipping. */
static Vertex vertex_cache[TR_MESH_VERTEX_LIMIT];
static int16_t view_cache[TR_MESH_VERTEX_LIMIT][3];
static int32_t inverse_depth[FAR_Z + 1]; /* 2^36 / depth */
static int32_t floor_depth[TR_HEIGHT];   /* floor depth * 16 along each row */
static Listed scene[TR_SCENE_TRIANGLES];
/* One bit for each strip of TR_BAND_HEIGHT rows that a listed triangle
 * touches. Kept apart so that each strip scans little memory. */
static Pair scene_bands[TR_SCENE_TRIANGLES / 2];
_Static_assert(TR_SCENE_TRIANGLES % 2 == 0, "bands are tested in pairs");
static size_t scene_count;
static TRMaterial materials[MATERIAL_LIMIT];
static unsigned material_count;

static int dir_x(int d) { return d == 2 ? 1 : d == 4 ? -1 : 0; }
static int dir_z(int d) { return d == 1 ? 1 : d == 3 ? -1 : 0; }

static void prepare_tables(void)
{
    if (inverse_depth[NEAR_Z]) return;
    for (int z = NEAR_Z; z <= FAR_Z; z++)
        inverse_depth[z] = (int32_t)(((uint64_t)1 << 36) / (unsigned)z);
    /* The camera is level from side to side, so a horizontal plane has one
     * depth per screen row. Solve it once for the temple floor. */
    for (int row = 0; row < TR_HEIGHT; row++) {
        int64_t numerator = (int64_t)(FLOOR_Y - CAMERA_HEIGHT) * 4096 * FOCAL * 16;
        int32_t denominator = PITCH_COS * (CENTRE_Y - (row * 16 + 8)) - PITCH_SIN * FOCAL;
        int64_t depth = denominator < 0 ? numerator / denominator : FAR_Z * 16;
        floor_depth[row] = (int32_t)(depth < NEAR_Z * 16 ? NEAR_Z * 16 :
            depth > FAR_Z * 16 ? FAR_Z * 16 : depth);
    }
}

static void project(TRScreenVertex *out, int32_t x, int32_t y, int32_t depth)
{
    int32_t inverse = inverse_depth[depth];
    out->x = CENTRE_X + (int32_t)(((int64_t)(x * FOCAL) * inverse) >> 36);
    out->y = CENTRE_Y - (int32_t)(((int64_t)(y * FOCAL) * inverse) >> 36);
    out->z = inverse;
}

/* Applies the camera pitch, classifies the vertex against the view and
 * projects it when it can be used directly. */
static inline void finish_vertex(size_t index, int32_t x, int32_t y, int32_t z)
{
    Vertex *out = &vertex_cache[index];
    y -= CAMERA_HEIGHT;
    int32_t up = (y * PITCH_COS + z * PITCH_SIN) >> 12;
    int32_t depth = (z * PITCH_COS - y * PITCH_SIN) >> 12;
    int32_t across = x * FOCAL, above = up * FOCAL;
    unsigned flags = 0;
    if (depth < NEAR_Z) flags = OUT_NEAR;
    else if (depth > FAR_Z) flags = OUT_FAR;
    if (across < -CENTRE_X * depth) flags |= OUT_LEFT;
    if (across > SCREEN_RIGHT * depth) flags |= OUT_RIGHT;
    if (above > CENTRE_Y * depth) flags |= OUT_TOP;
    if (above < -SCREEN_BOTTOM * depth) flags |= OUT_BOTTOM;
    int16_t *view = view_cache[index];
    view[0] = (int16_t)x; view[1] = (int16_t)up; view[2] = (int16_t)depth;
    /* A vertex in the view is well inside the guard band. */
    if (flags & (OUT_LEFT | OUT_RIGHT | OUT_TOP | OUT_BOTTOM)) {
        int32_t limit = GUARD * depth;
        if (across > limit || across < -limit || above > limit || above < -limit)
            flags |= OUT_GUARD;
    }
    if (!(flags & (OUT_NEAR | OUT_FAR | OUT_GUARD)))
        project(&out->screen, x, up, depth);
    out->flags = flags;
}

static void transform_mesh(const TRMesh *mesh, const Instance *instance)
{
    if (instance->runner) {
        for (size_t i = 0; i < mesh->position_count; i++) {
            const int16_t *v = mesh->positions[i];
            int32_t y = v[1], z = v[2];
            if (instance->sliding) { /* until the real animation is imported */
                int32_t above_ground = y - FLOOR_Y;
                y = FLOOR_Y + above_ground * 2 / 5;
                z += above_ground * 3 / 5;
            }
            finish_vertex(i, v[0], y + instance->height, z);
        }
        return;
    }
    int32_t vx = instance->view_x, vz = instance->view_z;
    for (size_t i = 0; i < mesh->position_count; i++) {
        const int16_t *v = mesh->positions[i];
        int32_t x = v[0] + instance->offset_x, z = v[2] + instance->offset_z;
        finish_vertex(i, (x * vz - z * vx) >> 12, v[1],
            ((x * vx + z * vz) >> 12) + CAMERA_BACK);
    }
}

/* Skip whole meshes that cannot reach the screen. */
static bool mesh_visible(const TRMesh *mesh, const Instance *instance)
{
    if (!mesh->radius) return true;
    int32_t x = mesh->centre[0] + instance->offset_x;
    int32_t z = mesh->centre[2] + instance->offset_z;
    int32_t side = (x * instance->view_z - z * instance->view_x) >> 12;
    int32_t ahead = ((x * instance->view_x + z * instance->view_z) >> 12) + CAMERA_BACK;
    int32_t y = mesh->centre[1] - CAMERA_HEIGHT;
    int32_t up = (y * PITCH_COS + ahead * PITCH_SIN) >> 12;
    int32_t depth = (ahead * PITCH_COS - y * PITCH_SIN) >> 12;
    int32_t r = mesh->radius + 4; /* allow for rounding */
    if (depth + r < NEAR_Z || depth - r > FAR_Z) return false;
    /* Distances to the four sides of the view, scaled by the normals. */
    int32_t across = side * FOCAL_PIXELS, above = up * FOCAL_PIXELS;
    int32_t edge = TR_WIDTH / 2 * depth;
    if (across - edge > r * SIDE_NORMAL || -across - edge > r * SIDE_NORMAL) return false;
    return above - HORIZON_ROW * depth <= r * TOP_NORMAL &&
        -above - (TR_HEIGHT - HORIZON_ROW) * depth <= r * BOTTOM_NORMAL;
}

static int32_t plane_distance(const ClipVertex *p, int plane)
{
    switch (plane) {
        case 0: return p->z - NEAR_Z;
        case 1: return GUARD * p->z - FOCAL * p->x;
        case 2: return GUARD * p->z + FOCAL * p->x;
        case 3: return GUARD * p->z - FOCAL * p->y;
        default: return GUARD * p->z + FOCAL * p->y;
    }
}

static int32_t between(int32_t a, int32_t b, uint32_t t)
{
    return a + (int32_t)(((int64_t)(b - a) * t) >> 30);
}

static int clip_plane(const ClipVertex *in, int count, ClipVertex *out, int plane)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        const ClipVertex *a = &in[i], *b = &in[i + 1 == count ? 0 : i + 1];
        int32_t da = plane_distance(a, plane), db = plane_distance(b, plane);
        if (da >= 0) out[n++] = *a;
        if ((da >= 0) != (db >= 0)) {
            /* Always interpolate from the inside end, so that triangles which
             * share this edge get the same new vertex and leave no crack. */
            const ClipVertex *p = da >= 0 ? a : b, *q = da >= 0 ? b : a;
            uint32_t inside = (uint32_t)(da >= 0 ? da : db);
            uint32_t total = inside - (uint32_t)(da >= 0 ? db : da);
            int shift;
            uint32_t r = tr_reciprocal(total, &shift);
            uint32_t t = (uint32_t)(((uint64_t)(inside << shift) * r) >> 17);
            ClipVertex v = {between(p->x, q->x, t), between(p->y, q->y, t),
                between(p->z, q->z, t), between(p->u, q->u, t), between(p->v, q->v, t)};
            out[n++] = v;
        }
    }
    return n;
}

/* The bits of the strips that hold rows top to bottom - 1 (not empty). */
static unsigned band_bits(int top, int bottom)
{
    return (2u << ((unsigned)(bottom - 1) / TR_BAND_HEIGHT)) -
        (1u << ((unsigned)top / TR_BAND_HEIGHT));
}

/* Only a few combinations occur, so each is stored once. */
static unsigned material_index(const TRMaterial *material)
{
    for (unsigned i = 0; i < material_count; i++) {
        const TRMaterial *m = &materials[i];
        if (m->texture == material->texture && m->floor_depth == material->floor_depth &&
            m->color == material->color && m->cull_back == material->cull_back) return i;
    }
    if (material_count == MATERIAL_LIMIT) return MATERIAL_LIMIT - 1;
    materials[material_count] = *material;
    return material_count++;
}

static inline void list_triangle(const TRScreenVertex *a, const TRScreenVertex *b,
    const TRScreenVertex *c, const TRTexCoord uv[3], unsigned material, bool cull)
{
    TRBounds bounds;
    if (!tr_triangle_rows(a, b, c, cull, &bounds)) return;
    if (scene_count == TR_SCENE_TRIANGLES) return;
    /* Writing the first of a pair clears its partner. */
    if (!(scene_count & 1)) scene_bands[scene_count / 2].both = 0;
    scene_bands[scene_count / 2].one[scene_count & 1] =
        (uint16_t)band_bits(bounds.top, bounds.bottom);
    Listed *out = &scene[scene_count++];
    out->x[0] = (int16_t)a->x; out->y[0] = (int16_t)a->y; out->z[0] = a->z;
    out->x[1] = (int16_t)b->x; out->y[1] = (int16_t)b->y; out->z[1] = b->z;
    out->x[2] = (int16_t)c->x; out->y[2] = (int16_t)c->y; out->z[2] = c->z;
    out->uv[0] = uv[0]; out->uv[1] = uv[1]; out->uv[2] = uv[2];
    out->bounds = bounds;
    out->material = (uint8_t)material;
}

/* Clip in view space, so triangles spanning the near plane (especially the
 * floor under the camera) do not disappear or distort. */
static void list_clipped(const TRTriangle *triangle, unsigned material, bool cull)
{
    ClipVertex buffer[2][2 * CLIP_LIMIT], *in = buffer[0], *out = buffer[1];
    for (int i = 0; i < 3; i++) {
        const int16_t *v = view_cache[triangle->corner[i]];
        ClipVertex c = {v[0], v[1], v[2], triangle->uv[i].u, triangle->uv[i].v};
        in[i] = c;
    }
    int count = 3;
    for (int plane = 0; plane < 5; plane++) {
        count = clip_plane(in, count, out, plane);
        /* Three corners and five planes give at most eight, barring rounding. */
        if (count < 3 || count > CLIP_LIMIT) return;
        ClipVertex *swap = in; in = out; out = swap;
    }
    TRScreenVertex screen[CLIP_LIMIT];
    for (int i = 0; i < count; i++) {
        int32_t depth = in[i].z < NEAR_Z ? NEAR_Z : in[i].z > FAR_Z ? FAR_Z : in[i].z;
        project(&screen[i], in[i].x, in[i].y, depth);
    }
    for (int i = 1; i + 1 < count; i++) {
        TRTexCoord uv[3] = {{(int16_t)in[0].u, (int16_t)in[0].v},
            {(int16_t)in[i].u, (int16_t)in[i].v},
            {(int16_t)in[i + 1].u, (int16_t)in[i + 1].v}};
        list_triangle(&screen[0], &screen[i], &screen[i + 1], uv, material, cull);
    }
}

/* Combine the two original corner meshes. Remove their walls that would
 * otherwise close the opposite exit; no second path mesh is stored. */
static bool junction_face(const TRMesh *mesh, const Instance *instance,
    const TRTriangle *triangle)
{
    int x = 0, z = 0;
    for (int i = 0; i < 3; i++) {
        x += mesh->positions[triangle->corner[i]][0];
        z += mesh->positions[triangle->corner[i]][2];
    }
    int side = x * dir_z(instance->incoming) - z * dir_x(instance->incoming);
    /* Split across the full depth of the corner. Restricting this split to
     * the far side also removed the near wall of the right-hand exit. */
    if (instance->junction_part == 1) return side <= 384;
    return side >= 384;
}

static void list_mesh(const TRMesh *mesh, const Instance *instance, uint16_t base)
{
    if (!mesh || !mesh->positions || !mesh->triangles ||
        mesh->position_count > TR_MESH_VERTEX_LIMIT) return;
    if (!instance->runner && !mesh_visible(mesh, instance)) return;
    transform_mesh(mesh, instance);
    /* The assembled junction shows some walls from behind. */
    bool cull = !instance->junction_part;
    TRMaterial plain = {mesh->texture, NULL, mesh->texture ? 0 : base, cull};
    unsigned plain_index = material_index(&plain), floor_index = plain_index;
    if (instance->floor) {
        TRMaterial floor = plain;
        floor.floor_depth = floor_depth;
        floor_index = material_index(&floor);
    }
    const TRTriangle *end = mesh->triangles + mesh->triangle_count;
    for (const TRTriangle *t = mesh->triangles; t != end; t++) {
        const Vertex *a = &vertex_cache[t->corner[0]], *b = &vertex_cache[t->corner[1]],
            *c = &vertex_cache[t->corner[2]];
        if (a->flags & b->flags & c->flags & OUTSIDE) continue;
        unsigned any = a->flags | b->flags | c->flags;
        if (any & OUT_FAR) continue;
        if (instance->junction_part && !junction_face(mesh, instance, t)) continue;
        unsigned material = t->floor ? floor_index : plain_index;
        if (any & (OUT_NEAR | OUT_GUARD)) list_clipped(t, material, cull);
        else list_triangle(&a->screen, &b->screen, &c->screen, t->uv, material, cull);
    }
}

/* The reference temple floor is a flat plane at this prefab-local height.
 * Walls, coins and other meshes use affine texture mapping. */
static bool is_floor_face(const TRMesh *mesh, const TRTriangle *triangle)
{
    for (int i = 0; i < 3; i++) {
        int y = mesh->positions[triangle->corner[i]][1];
        if (y < FLOOR_Y - 1 || y > FLOOR_Y + 1) return false;
    }
    return true;
}

static void mesh_bounds(TRMesh *mesh)
{
    mesh->radius = 0;
    if (!mesh->position_count) return;
    int32_t low[3], high[3];
    for (int k = 0; k < 3; k++) low[k] = high[k] = mesh->positions[0][k];
    for (size_t i = 1; i < mesh->position_count; i++) for (int k = 0; k < 3; k++) {
        if (mesh->positions[i][k] < low[k]) low[k] = mesh->positions[i][k];
        if (mesh->positions[i][k] > high[k]) high[k] = mesh->positions[i][k];
    }
    for (int k = 0; k < 3; k++) mesh->centre[k] = (int16_t)((low[k] + high[k]) / 2);
    int64_t furthest = 0;
    for (size_t i = 0; i < mesh->position_count; i++) {
        int64_t sum = 0;
        for (int k = 0; k < 3; k++) {
            int64_t d = mesh->positions[i][k] - mesh->centre[k];
            sum += d * d;
        }
        if (sum > furthest) furthest = sum;
    }
    int32_t radius = 0;
    for (int32_t bit = 1 << 16; bit; bit >>= 1)
        if ((int64_t)(radius + bit) * (radius + bit) <= furthest) radius += bit;
    mesh->radius = radius + 1;
}

size_t tr_mesh_build(TRMesh *mesh, const TRMeshSource *source, const TRTexture *texture,
    int16_t (*positions)[3], TRTriangle *triangles)
{
    size_t used = 0, count = 0;
    for (size_t i = 0; i + 2 < source->index_count; i += 3) {
        const uint16_t *index = &source->indices[i];
        if (index[0] >= source->vertex_count || index[1] >= source->vertex_count ||
            index[2] >= source->vertex_count) continue;
        TRTriangle *triangle = &triangles[count++];
        for (int k = 0; k < 3; k++) {
            const int16_t *v = source->positions[index[k]];
            size_t found = 0;
            while (found < used && (positions[found][0] != v[0] ||
                positions[found][1] != v[1] || positions[found][2] != v[2])) found++;
            if (found == used) {
                positions[used][0] = v[0]; positions[used][1] = v[1];
                positions[used][2] = v[2];
                used++;
            }
            triangle->corner[k] = (uint16_t)found;
            TRTexCoord uv = {0, 0};
            if (source->uvs) {
                uv.u = source->uvs[index[k]][0];
                uv.v = (int16_t)(2048 - source->uvs[index[k]][1]);
            }
            triangle->uv[k] = uv;
        }
    }
    *mesh = (TRMesh){.positions = (const int16_t (*)[3])positions, .triangles = triangles,
        .position_count = used, .triangle_count = count, .texture = texture};
    for (size_t i = 0; i < count; i++)
        triangles[i].floor = is_floor_face(mesh, &triangles[i]);
    mesh_bounds(mesh);
    return used;
}

/* Sky and ground, at depth zero so that any geometry covers them. */
static void clear_target(const TRTarget *target)
{
    _Static_assert(TR_WIDTH * 2 % 8 == 0 && HORIZON_ROW % 2 == 0,
        "an even number of rows must be whole cache lines");
    int horizon = HORIZON_ROW < target->top ? target->top :
        HORIZON_ROW > target->bottom ? target->bottom : HORIZON_ROW;
    if (horizon > target->top)
        tr_fill(target->pixels, (uint32_t)((horizon - target->top) * TR_WIDTH / 8), SKY);
    if (target->bottom > horizon)
        tr_fill(target->pixels + (horizon - target->top) * TR_WIDTH,
            (uint32_t)((target->bottom - horizon) * TR_WIDTH / 8), GROUND);
}

static int corner_index(const TRTile *tile, int sign)
{
    static const int left[] = {0,0,2,3,1};
    static const int right[] = {0,1,0,2,3};
    return sign < 0 ? left[tile->incoming] : right[tile->incoming];
}

typedef struct {
    Instance instance; /* camera facing, shared by every mesh */
    int32_t camera_x, camera_z;
} Camera;

static void list_at(const TRMesh *mesh, const Camera *camera, Instance instance,
    int x, int z, uint16_t color)
{
    instance.offset_x = x * 16 - camera->camera_x;
    instance.offset_z = z * 16 - camera->camera_z;
    list_mesh(mesh, &instance, color);
}

static void list_tile(const TRTile *tile, const TRAssets *assets, const Camera *camera)
{
    int axis = tile->incoming == 1 || tile->incoming == 3 ? 0 : 1;
    Instance ground = camera->instance;
    ground.floor = true;
    if (tile->junction) {
        ground.incoming = tile->incoming;
        ground.junction_part = 1;
        list_at(assets->corner[corner_index(tile, -1)], camera, ground,
            tile->x, tile->z, 0x7be9);
        ground.junction_part = 2;
        list_at(assets->corner[corner_index(tile, 1)], camera, ground,
            tile->x, tile->z, 0x7be9);
        ground.junction_part = 0;
        int exit_axis = tile->outgoing == 1 || tile->outgoing == 3 ? 0 : 1;
        list_at(assets->path[exit_axis], camera, ground,
            tile->x - 60 * dir_x(tile->outgoing),
            tile->z - 60 * dir_z(tile->outgoing), 0x7be9);
    } else {
        const TRMesh *mesh = tile->turn ? assets->corner[corner_index(tile, tile->turn)] :
            tile->terrain == 1 ? assets->hole[axis] :
            tile->terrain == 2 ? assets->narrow[axis] : assets->path[axis];
        list_at(mesh, camera, ground, tile->x, tile->z, 0x7be9);
    }
    if (tile->obstacle) {
        const TRMesh *obstacle = tile->obstacle == 1 ? assets->jump[axis] :
            tile->obstacle == 2 ? assets->slide[axis] : assets->trip[axis];
        list_at(obstacle, camera, camera->instance, tile->x, tile->z, 0x7be9);
    }
    if (tile->coin_lane != 2) for (int j = 0; j < 6; j++) {
        if (tile->coins_collected & (1u << j)) continue;
        int along = -24 + j * 9, lane = 6 * tile->coin_lane;
        int x = tile->x + along * dir_x(tile->incoming) + lane * dir_z(tile->incoming);
        int z = tile->z + along * dir_z(tile->incoming) - lane * dir_x(tile->incoming);
        list_at(assets->coin, camera, camera->instance, x, z, 0xfc00);
    }
}

void tr_scene_build(const TRCourse *course, const TRAssets *assets, const TRPose *pose,
    bool sliding)
{
    prepare_tables();
    scene_count = 0;
    material_count = 0;
    float view_x = pose->view_x, view_z = pose->view_z;
    if (view_x == 0 && view_z == 0) {
        view_x = (float)dir_x(course->heading); view_z = (float)dir_z(course->heading);
    }
    Camera camera = {{0}, (int32_t)(pose->x * 16), (int32_t)(pose->z * 16)};
    camera.instance.view_x = (int32_t)(view_x * 4096);
    camera.instance.view_z = (int32_t)(view_z * 4096);

    /* The runner is nearest, so drawing it first saves hidden floor pixels. */
    Instance runner = {.runner = true, .sliding = sliding,
        .height = (int32_t)(pose->height * 4.8f)};
    list_mesh(assets->player, &runner, 0x9a63);

    for (int i = 0; i < course->count; i++) {
        const TRTile *tile = &course->tiles[i];
        int32_t x = tile->x * 16 - camera.camera_x, z = tile->z * 16 - camera.camera_z;
        int32_t ahead = (x * camera.instance.view_x + z * camera.instance.view_z) >> 12;
        int32_t side = (x * camera.instance.view_z - z * camera.instance.view_x) >> 12;
        if (ahead < -70 * 16 || ahead > 280 * 16 || side > 150 * 16 || side < -150 * 16)
            continue;
        list_tile(tile, assets, &camera);
    }
}

static void draw_listed(const TRTarget *target, const Listed *t)
{
    TRScreenVertex a = {t->x[0], t->y[0], t->z[0]}, b = {t->x[1], t->y[1], t->z[1]},
        c = {t->x[2], t->y[2], t->z[2]};
    tr_raster_visible(target, &a, &b, &c, &t->bounds, t->uv, &materials[t->material]);
}

void tr_scene_draw(const TRTarget *target)
{
    clear_target(target);
    uint16_t wanted = (uint16_t)band_bits(target->top, target->bottom);
    uint32_t either = (uint32_t)wanted << 16 | wanted;
    /* Most triangles miss a given strip, so test two of them at a time. */
    const Pair *pair = scene_bands, *end = scene_bands + (scene_count + 1) / 2;
    for (const Listed *t = scene; pair != end; pair++, t += 2) {
        if (!(pair->both & either)) continue;
        if (pair->one[0] & wanted) draw_listed(target, t);
        if (pair->one[1] & wanted) draw_listed(target, t + 1);
    }
}

size_t tr_scene_size(void) { return scene_count; }
