#include "renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static TRFramebuffer a, b;

int main(void)
{
    const float points[][3] = {{-4,-4,0}, {4,-4,0}, {0,4,0}};
    const uint16_t good_indices[] = {0,1,2};
    const uint16_t bad_indices[] = {0,1,30};
    TRMesh player = {points,good_indices,3,3,NULL,NULL,NULL,NULL};
    TRMesh invalid = {points,bad_indices,3,3,NULL,NULL,NULL,NULL};
    tr_render_frame(&a, NULL, NULL, 0, 0, 0);
    tr_render_frame(&b, NULL, &invalid, 0, 0, 0);
    assert(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0);
    tr_render_frame(&b, NULL, &player, 0, 0, 0);
    int changed = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++) {
        if (a.pixels[i] != b.pixels[i]) {
            changed++;
            assert(b.depth[i] > 0);
        }
    }
    assert(changed > 10);
    /* Equivalent prefab-local positions exercise the target's integer path. */
    const int16_t fixed_points[][3] = {{-64,-122,480}, {64,-122,480}, {0,6,480}};
    const int16_t fixed_uvs[][2] = {{0,0}, {0,0}, {0,0}};
    TRMesh fixed_player = player;
    fixed_player.fixed_vertices = fixed_points;
    fixed_player.fixed_uvs = fixed_uvs;
    tr_render_frame(&b, NULL, &fixed_player, 0, 0, 0);
    changed = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        if (a.pixels[i] != b.pixels[i]) changed++;
    assert(changed > 5);
    const float offscreen_points[][3] = {{100,-4,0}, {104,-4,0}, {102,4,0}};
    TRMesh offscreen = {offscreen_points,good_indices,3,3,NULL,NULL,NULL,NULL};
    tr_render_frame(&b, NULL, &offscreen, 0, 0, 0);
    assert(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0);
    static uint16_t texture[128*128];
    const float uv[][2] = {{0.2f,0.2f},{0.2f,0.2f},{0.2f,0.2f}};
    for (int i = 0; i < 128*128; i++) texture[i] = 0xf800;
    player.uvs = uv;
    player.texture = texture;
    tr_render_frame(&b, NULL, &player, 0, 0, 0);
    int red = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        if (b.pixels[i] == 0xf800) red++;
    assert(red > 10);
    const float near_points[][3] = {{-0.2f,3.4f,-28},{0.2f,3.4f,-26},{0,3.8f,-26}};
    TRMesh clipped = {near_points,good_indices,3,3,NULL,NULL,NULL,NULL};
    tr_render_frame(&b, NULL, &clipped, 0, 0, 0);
    changed = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        if (a.pixels[i] != b.pixels[i]) changed++;
    assert(changed > 0);
    TRCourse course;
    memset(&course, 0, sizeof(course));
    course.count = 1; course.heading = 1; course.z = -15;
    course.tiles[0].incoming = 1;
    course.tiles[0].coin_lane = 0;
    TRAssets assets = {0}; assets.coin = &player;
    tr_render_world(&b, &course, &assets, 0, false);
    changed = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        if (a.pixels[i] != b.pixels[i]) changed++;
    assert(changed > 0);
    course.tiles[0].coins_collected = 63;
    tr_render_world(&b, &course, &assets, 0, false);
    assert(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0);
    const float corner_points[][3] = {
        {-633.9522f, 1272.3496f, -15}, {-625.9522f, 1272.3496f, -15},
        {-629.9522f, 1278.3496f, -15}
    };
    TRMesh corner = {corner_points,good_indices,3,3,NULL,NULL,NULL,NULL};
    course.z = 620; course.tiles[0].z = 660; course.tiles[0].turn = -1;
    assets.corner[0] = &corner;
    tr_render_world(&b, &course, &assets, 0, false);
    changed = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        if (a.pixels[i] != b.pixels[i]) changed++;
    assert(changed > 0);
    course.tiles[0].turn = 1;
    tr_render_world(&b, &course, &assets, 0, false);
    assert(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0);
    const float hole_points[][3] = {
        {-1143.9521f, 922.3496f, -15}, {-1135.9521f, 922.3496f, -15},
        {-1139.9521f, 928.3496f, -15}
    };
    TRMesh hole = {hole_points,good_indices,3,3,NULL,NULL,NULL,NULL};
    course.tiles[0].turn = 0; course.tiles[0].terrain = 1;
    assets.hole[0] = &hole;
    tr_render_world(&b, &course, &assets, 0, false);
    changed = 0;
    for (int i = 0; i < TR_WIDTH * TR_HEIGHT; i++)
        if (a.pixels[i] != b.pixels[i]) changed++;
    assert(changed > 0);
    course.tiles[0].terrain = 2;
    tr_render_world(&b, &course, &assets, 0, false);
    assert(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0);
    puts("Mesh renderer checks passed");
    return 0;
}
