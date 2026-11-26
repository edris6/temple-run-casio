/* Exercise the private vertex stage and scene traversal with synthetic meshes. */
#include "../src/renderer.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static TRFramebuffer empty, a, b;
static uint32_t state = 987654321;

static int between_values(int low, int high)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return low + (int)(state % (uint32_t)(high - low + 1));
}

typedef struct {
    TRMesh mesh;
    int16_t positions[96][3];
    TRTriangle triangles[32];
} Built;

static const TRMesh *build(Built *out, const int16_t (*positions)[3], size_t vertices,
    const uint16_t *indices, size_t count, const TRTexture *texture)
{
    static const int16_t uvs[96][2] = {{0}};
    TRMeshSource source = {positions, uvs, indices, vertices, count};
    tr_mesh_build(&out->mesh, &source, texture, out->positions, out->triangles);
    return &out->mesh;
}

static int changed(const TRFramebuffer *x, const TRFramebuffer *y)
{
    int n = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) n += x->pixels[i] != y->pixels[i];
    return n;
}

/* One mesh as the runner, or as an obstacle at the camera position. */
static void render_runner(TRFramebuffer *fb, const TRMesh *mesh)
{
    TRCourse course;
    memset(&course, 0, sizeof course);
    course.heading = 1;
    TRAssets assets = {0};
    assets.player = mesh;
    TRPose pose = {0, 0, 0, 1, 0};
    tr_render_world(fb, &course, &assets, &pose, false);
}

static void check_build(void)
{
    /* Two triangles that repeat positions, and one with a bad index. */
    static const int16_t positions[6][3] = {
        {0,-144,0}, {16,-144,0}, {0,-144,16}, {16,-144,0}, {0,-100,0}, {0,-144,16}};
    static const int16_t uvs[6][2] = {{0,0},{2048,0},{0,2048},{5,6},{7,8},{9,10}};
    static const uint16_t indices[] = {0,1,2, 3,4,5, 0,1,30};
    TRMeshSource source = {positions, uvs, indices, 6, 9};
    Built built;
    size_t used = tr_mesh_build(&built.mesh, &source, NULL, built.positions, built.triangles);
    assert(used == 4 && built.mesh.position_count == 4);
    assert(built.mesh.triangle_count == 2);
    assert(built.triangles[0].corner[1] == built.triangles[1].corner[0]);
    assert(built.triangles[0].corner[2] == built.triangles[1].corner[2]);
    assert(built.triangles[0].floor && !built.triangles[1].floor);
    /* Texture coordinates stay with their corner; v is turned downward. */
    assert(built.triangles[1].uv[0].u == 5 && built.triangles[1].uv[0].v == 2042);
    assert(built.triangles[0].uv[2].v == 0);
    for (size_t i = 0; i < used; i++) {
        int64_t distance = 0;
        for (int k = 0; k < 3; k++) {
            int64_t d = built.positions[i][k] - built.mesh.centre[k];
            distance += d * d;
        }
        assert(distance <= (int64_t)built.mesh.radius * built.mesh.radius);
    }
}

static void check_runner_views(void)
{
    static const int16_t visible[3][3] = {{-64,-122,480}, {64,-122,480}, {0,6,480}};
    static const int16_t offscreen[3][3] = {{3000,-122,480}, {3128,-122,480}, {3064,6,480}};
    static const int16_t behind[3][3] = {{-64,-122,-480}, {64,-122,-480}, {0,6,-480}};
    static const int16_t distant[3][3] = {{-640,-122,7000}, {640,-122,7000}, {0,600,7000}};
    /* A floor triangle reaching from behind the camera into view. */
    static const int16_t near_plane[3][3] = {{-80,-144,-100}, {80,-144,-100}, {0,-144,900}};
    static const uint16_t front[] = {0,2,1}, back[] = {0,1,2}, bad[] = {0,1,30};
    static uint16_t red[128 * 128];
    for (int i = 0; i < 128 * 128; i++) red[i] = 0xf800;
    TRTexture texture = {{red, red, red}};
    Built mesh;
    render_runner(&empty, NULL);
    render_runner(&b, build(&mesh, visible, 3, bad, 3, NULL));
    assert(!changed(&empty, &b));
    /* Exactly one winding faces the camera. */
    render_runner(&a, build(&mesh, visible, 3, front, 3, &texture));
    render_runner(&b, build(&mesh, visible, 3, back, 3, &texture));
    assert((changed(&empty, &a) > 20) != (changed(&empty, &b) > 20));
    const uint16_t *winding = changed(&empty, &a) ? front : back;
    render_runner(&b, build(&mesh, visible, 3, winding, 3, &texture));
    int red_pixels = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        red_pixels += tr_pixel_color(b.pixels[i]) == 0xf800;
    assert(red_pixels > 20);
    const int16_t (*hidden[])[3] = {offscreen, behind, distant};
    for (int i = 0; i < 3; i++) {
        render_runner(&b, build(&mesh, hidden[i], 3, winding, 3, &texture));
        assert(!changed(&empty, &b));
    }
    int most = 0;
    for (int i = 0; i < 2; i++) {
        render_runner(&b, build(&mesh, near_plane, 3, i ? back : front, 3, &texture));
        if (changed(&empty, &b) > most) most = changed(&empty, &b);
    }
    assert(most > 200);
}

static void check_tiles(void)
{
    static const int16_t shape[3][3] = {{-64,-100,0}, {64,-100,0}, {0,6,0}};
    static const uint16_t both[] = {0,1,2, 0,2,1};
    Built mesh;
    build(&mesh, shape, 3, both, 6, NULL);
    TRCourse course;
    memset(&course, 0, sizeof course);
    course.count = 1; course.heading = 1;
    course.tiles[0].incoming = course.tiles[0].outgoing = 1;
    TRPose pose = {0, -15, 0, 1, 0};
    TRAssets assets = {0};
    tr_render_world(&empty, &course, &assets, &pose, false);

    assets.coin = &mesh.mesh;
    course.tiles[0].coin_lane = 0;
    tr_render_world(&b, &course, &assets, &pose, false);
    assert(changed(&empty, &b) > 0);
    course.tiles[0].coins_collected = 63;
    tr_render_world(&b, &course, &assets, &pose, false);
    assert(!changed(&empty, &b));
    course.tiles[0].coin_lane = 2;

    /* Each kind of tile draws only its own mesh. */
    assets = (TRAssets){0}; assets.corner[0] = &mesh.mesh;
    course.tiles[0].turn = -1;
    tr_render_world(&b, &course, &assets, &pose, false);
    assert(changed(&empty, &b) > 0);
    course.tiles[0].turn = 1;
    tr_render_world(&b, &course, &assets, &pose, false);
    assert(!changed(&empty, &b));
    course.tiles[0].turn = 0;
    assets = (TRAssets){0}; assets.hole[0] = &mesh.mesh;
    course.tiles[0].terrain = 1;
    tr_render_world(&b, &course, &assets, &pose, false);
    assert(changed(&empty, &b) > 0);
    course.tiles[0].terrain = 2;
    tr_render_world(&b, &course, &assets, &pose, false);
    assert(!changed(&empty, &b));
}

static void check_junction(void)
{
    /* Both halves of the junction must include their near-side walls as well
     * as their far-side geometry, whichever way the runner approaches. */
    for (int heading = 1; heading <= 4; heading++) {
        int dx = dir_x(heading), dz = dir_z(heading);
        int16_t points[6][3];
        for (int i = 0; i < 6; i++) {
            int side = i < 3 ? 12 : -12, ahead = -20;
            points[i][0] = (int16_t)(16*(side*dz + ahead*dx));
            points[i][1] = 0;
            points[i][2] = (int16_t)(16*(-side*dx + ahead*dz));
        }
        TRMesh part = {0}; part.positions = points;
        TRTriangle right = {.corner = {0,1,2}}, left = {.corner = {3,4,5}};
        Instance instance = {.junction_part = 1, .incoming = heading};
        assert(!junction_face(&part, &instance, &right));
        assert(junction_face(&part, &instance, &left));
        instance.junction_part = 2;
        assert(junction_face(&part, &instance, &right));
        assert(!junction_face(&part, &instance, &left));
        /* The middle of the corner belongs to the first half only, and faces
         * on the dividing line to both. */
        for (int offset = 0; offset <= 8; offset += 8) {
            for (int i = 0; i < 3; i++) {
                points[i][0] = (int16_t)(16*(offset*dz - 20*dx));
                points[i][2] = (int16_t)(16*(-offset*dx - 20*dz));
            }
            instance.junction_part = 1;
            assert(junction_face(&part, &instance, &right));
            instance.junction_part = 2;
            assert(junction_face(&part, &instance, &right) == (offset == 8));
        }
    }
}

/* Random geometry all around the camera, facing any direction: skipping whole
 * meshes by their bounds must never change the picture. */
static void check_culling_is_invisible(void)
{
    static uint16_t indices[96];
    for (int i = 0; i < 96; i++) indices[i] = (uint16_t)i;
    TRCourse course;
    memset(&course, 0, sizeof course);
    course.count = 1; course.heading = 1;
    course.tiles[0].incoming = course.tiles[0].outgoing = 1;
    course.tiles[0].coin_lane = 2; course.tiles[0].obstacle = 1;
    int drawn = 0;
    for (int round = 0; round < 20000; round++) {
        int16_t points[24][3];
        int spread = between_values(0, 1) ? between_values(4, 100) : between_values(100, 3000);
        int cx = between_values(-1500, 1500), cy = between_values(-800, 800);
        int cz = between_values(-1500, 1500);
        for (int i = 0; i < 24; i++) {
            points[i][0] = (int16_t)(cx + between_values(-spread, spread));
            points[i][1] = (int16_t)(cy + between_values(-spread, spread));
            points[i][2] = (int16_t)(cz + between_values(-spread, spread));
        }
        Built mesh;
        build(&mesh, points, 24, indices, 24, NULL);
        double angle = between_values(0, 359) * 3.14159265358979 / 180;
        TRPose pose = {(float)between_values(-60, 140), (float)between_values(-60, 140),
            (float)sin(angle), (float)cos(angle), 0};
        /* The tile itself must pass the traversal's own distance test. */
        course.tiles[0].x = (int)(pose.x + 40 * pose.view_x);
        course.tiles[0].z = (int)(pose.z + 40 * pose.view_z);
        TRAssets assets = {0}; assets.jump[0] = &mesh.mesh;
        tr_render_world(&a, &course, &assets, &pose, false);
        mesh.mesh.radius = 0;
        tr_render_world(&b, &course, &assets, &pose, false);
        assert(!changed(&a, &b));
        drawn += changed(&a, &empty) > 0;
    }
    assert(drawn > 1000);
}

int main(void)
{
    check_build();
    check_runner_views();
    check_tiles();
    check_junction();
    check_culling_is_invisible();
    puts("Mesh renderer checks passed");
    return 0;
}
