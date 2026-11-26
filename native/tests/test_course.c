#include "course.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static TRCourse empty_course(void)
{
    TRCourse c;
    memset(&c, 0, sizeof(c));
    c.count = TR_OPENING_TILES; c.heading = 1;
    for (int i = 0; i < TR_OPENING_TILES; i++) {
        c.tiles[i].index = i; c.tiles[i].z = i*60;
        c.tiles[i].incoming = c.tiles[i].outgoing = 1; c.tiles[i].coin_lane = 2;
    }
    return c;
}

static int test_dx(int heading) { return heading == 2 ? 1 : heading == 4 ? -1 : 0; }
static int test_dz(int heading) { return heading == 1 ? 1 : heading == 3 ? -1 : 0; }

/* Check generated challenges across seeds, speeds and many lookahead refills. */
static void check_spacing(int seed, float distance)
{
    TRCourse c; TRRunner runner;
    tr_course_init(&c, seed); tr_reset(&runner);
    runner.distance = distance;
    int last_index = -1, last_challenge = -TR_CHALLENGE_SPACING;
    bool narrow = false, hole = false, slide = false;
    while (c.generated < 360) {
        for (int i = 0; i < c.count; i++) {
            TRTile *tile = &c.tiles[i];
            if (tile->index > last_index) {
                if (tile->turn || tile->terrain || tile->obstacle) {
                    assert(tile->index - last_challenge >= TR_CHALLENGE_SPACING);
                    last_challenge = tile->index;
                }
                if (tile->index == 17) narrow = tile->terrain == 2;
                if (tile->index == 23) hole = tile->terrain == 1;
                if (tile->index == 29) slide = tile->obstacle == 2;
                last_index = tile->index;
            }
            /* Move the generator forward without simulating a player. */
            tile->obstacle = 0; tile->terrain = 0; tile->coin_lane = 2;
        }
        c.current = 9;
        TRTile *tile = &c.tiles[c.current];
        tile->turned = true;
        c.x = (float)tile->x; c.z = (float)tile->z;
        tr_course_step(&c, &runner, runner.forward, 0, 0);
        assert(!runner.dead && c.count <= TR_COURSE_CAPACITY);
    }
    assert(narrow && hole && slide);
}

int main(void)
{
    TRCourse a, b;
    tr_course_init(&a, 1);
    tr_course_init(&b, 1);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    /* The port changes the original host scheduler to space challenges. */
    assert(a.tiles[11].turn == -1 && a.tiles[11].outgoing == 4);
    assert(a.tiles[12].x == -60 && a.tiles[12].z == 660 && a.tiles[12].incoming == 4);
    tr_course_init(&b, INT_MAX);
    tr_course_init(&b, INT_MIN);
    for (int i = 0; i < 4; i++) assert(a.tiles[i].obstacle == 0);
    for (int i = 0; i < TR_OPENING_TILES-1; i++) {
        assert(a.tiles[i].obstacle == 0 || a.tiles[i].obstacle == 1 || a.tiles[i].obstacle == 3);
        if (a.tiles[i].obstacle) assert(a.tiles[i].coin_lane == 2);
        if (a.tiles[i].coin_lane != 2) assert(i > 0 && i % 6 < 4);
    }

    TRRunner runner;
    TRCourse c = empty_course();
    c.tiles[2].obstacle = 1;
    tr_reset(&runner); runner.forward = 104; c.z = 122;
    tr_course_step(&c, &runner, 102, 0, 0);
    assert(runner.dead && c.tiles[2].hit);
    c = empty_course(); c.tiles[2].obstacle = 1;
    tr_reset(&runner); runner.forward = 104; runner.height = 6; c.z = 122;
    tr_course_step(&c, &runner, 102, 0, 0);
    assert(!runner.dead && !c.tiles[2].hit);

    c = empty_course(); c.tiles[2].obstacle = 2;
    tr_reset(&runner); runner.forward = 98; c.z = 116;
    tr_course_step(&c, &runner, 96, 0, 0);
    assert(runner.dead);
    c = empty_course(); c.tiles[2].obstacle = 2;
    tr_reset(&runner); runner.forward = 98; runner.sliding = true; c.z = 116;
    tr_course_step(&c, &runner, 96, 0, 0);
    assert(!runner.dead);

    c = empty_course(); c.tiles[2].obstacle = 3;
    tr_reset(&runner); runner.forward = 100; runner.stumble_kill_timer = 10; c.z = 118;
    tr_course_step(&c, &runner, 98, 0, 0);
    assert(!runner.dead && runner.stumbling && runner.speed == 85);

    c = empty_course(); c.current = 6; c.tiles[6].coin_lane = 0;
    tr_reset(&runner); runner.forward = 316; c.z = 335;
    tr_course_step(&c, &runner, 315, 0, 0);
    assert(runner.coins == 1 && c.tiles[6].coins_collected == 1);
    tr_course_step(&c, &runner, 316, 0, 0);
    assert(runner.coins == 1);
    c = empty_course(); c.current = 6; c.tiles[6].coin_lane = 1;
    tr_reset(&runner); runner.forward = 316; c.z = 335;
    tr_course_step(&c, &runner, 315, 0, 0);
    assert(runner.coins == 0);

    tr_course_init(&c, 1); tr_reset(&runner); runner.stumble_kill_timer = 10;
    c.current = 10; c.z = 610;
    float lateral = 4;
    tr_course_request_turn(&c, &runner, &lateral, -1);
    assert(c.buffered_turn == -1);
    c.current = 11; c.z = 650;
    tr_course_process_turn(&c, &runner, &lateral, 1.0f/30.0f);
    assert(c.tiles[11].turned && c.heading == 4 && c.z == 660 && lateral == 0);

    tr_course_init(&c, 1); tr_reset(&runner); runner.stumble_kill_timer = 10;
    c.current = 11; c.z = 660; c.tiles[11].junction = false; lateral = 0;
    tr_course_request_turn(&c, &runner, &lateral, 1);
    assert(runner.stumbling && !c.tiles[11].turned);
    tr_course_init(&c, 1); tr_reset(&runner);
    c.current = 11; c.z = 689; runner.forward = 2;
    tr_course_step(&c, &runner, 0, 0, 0);
    assert(runner.dead);

    c = empty_course(); c.current = 2; c.heading = 2;
    c.tiles[2].x = 120; c.tiles[2].z = 0;
    c.tiles[2].incoming = c.tiles[2].outgoing = 2; c.tiles[2].obstacle = 1;
    c.x = 112; c.z = 0; tr_reset(&runner); runner.forward = 2;
    tr_course_step(&c, &runner, 0, 0, 0);
    assert(runner.dead); /* horizontal prefab region spans x=-7..-3 */

    /* Both D-pad directions select a connected course at each orientation. */
    for (int heading = 1; heading <= 4; heading++) for (int sign = -1; sign <= 1; sign += 2) {
        memset(&c, 0, sizeof(c)); tr_reset(&runner); lateral = 3;
        int default_heading = (heading + 2) % 4 + 1;
        int selected_heading = sign < 0 ? default_heading : heading % 4 + 1;
        c.count = 4; c.heading = heading; c.x = 600; c.z = -400;
        c.tiles[0] = (TRTile){.x=600, .z=-400, .incoming=heading,
            .outgoing=default_heading, .turn=-1, .junction=true, .coin_lane=2};
        for (int i = 1; i < c.count; i++) {
            c.tiles[i] = (TRTile){.x=600 + i*60*test_dx(default_heading),
                .z=-400 + i*60*test_dz(default_heading),
                .incoming=default_heading, .outgoing=default_heading, .coin_lane=2};
        }
        assert(tr_course_ground_at(&c, 600 + 20*test_dx(default_heading),
            -400 + 20*test_dz(default_heading)));
        int opposite = (default_heading + 1) % 4 + 1;
        assert(tr_course_ground_at(&c, 600 + 20*test_dx(opposite),
            -400 + 20*test_dz(opposite)));
        tr_course_request_turn(&c, &runner, &lateral, sign);
        assert(!runner.dead && c.tiles[0].turned && c.heading == selected_heading && lateral == 0);
        for (int i = 1; i < c.count; i++) {
            assert(c.tiles[i].x == 600 + i*60*test_dx(selected_heading));
            assert(c.tiles[i].z == -400 + i*60*test_dz(selected_heading));
            assert(c.tiles[i].incoming == selected_heading && c.tiles[i].outgoing == selected_heading);
        }
    }

    tr_course_init(&c, 1); tr_reset(&runner);
    c.current = 6; c.z = 360; c.tiles[6].obstacle = 0;
    tr_course_step(&c, &runner, runner.forward, 0, 0);
    assert(c.tiles[17].terrain == 2);
    for (int seed = 0; seed < 100; seed++) {
        check_spacing(seed, 0);
        check_spacing(seed, 500);
        check_spacing(seed, 2500);
    }
    /* The Region2D ground shapes come directly from the track prefabs. */
    TRTile terrain = {.x=100, .z=200, .incoming=1, .terrain=1};
    assert(tr_terrain_ground(&terrain, 100, 173));
    assert(tr_terrain_ground(&terrain, 100, 227));
    assert(!tr_terrain_ground(&terrain, 100, 200));
    assert(!tr_terrain_ground(&terrain, 112, 227));
    terrain.incoming = 4;
    assert(tr_terrain_ground(&terrain, 73, 200));
    assert(tr_terrain_ground(&terrain, 127, 200));
    assert(!tr_terrain_ground(&terrain, 100, 200));
    terrain.terrain = 2; terrain.incoming = 3;
    assert(tr_terrain_ground(&terrain, 95, 200));
    assert(!tr_terrain_ground(&terrain, 105, 200));
    terrain.incoming = 2;
    assert(tr_terrain_ground(&terrain, 100, 205));
    assert(!tr_terrain_ground(&terrain, 100, 195));

    c = empty_course(); c.current = 2; c.tiles[2].terrain = 1;
    assert(tr_course_ground_at(&c, 0, 92));
    assert(!tr_course_ground_at(&c, 0, 120));
    assert(tr_course_ground_at(&c, 0, 148));

    tr_course_init(&c, 1); tr_reset(&runner); runner.stumble_kill_timer = 10;
    int turns = 0;
    for (int step = 0; step < 3000; step++) {
        for (int i = 0; i < c.count; i++) {
            c.tiles[i].obstacle = c.tiles[i].terrain = 0; c.tiles[i].coin_lane = 2;
        }
        TRTile *tile = &c.tiles[c.current];
        float ahead = (tile->x-c.x)*(tile->incoming == 2 ? 1 : tile->incoming == 4 ? -1 : 0) +
            (tile->z-c.z)*(tile->incoming == 1 ? 1 : tile->incoming == 3 ? -1 : 0);
        if (tile->turn && !tile->turned && ahead >= -10 && ahead <= 10) {
            lateral = 0; tr_course_request_turn(&c, &runner, &lateral, tile->turn); turns++;
        }
        float previous = runner.forward; runner.forward += 3.0f;
        tr_course_step(&c, &runner, previous, 0, 0);
        assert(!runner.dead && c.count <= TR_COURSE_CAPACITY);
    }
    assert(turns > 5 && c.generated > 100);
    puts("Opening course checks passed");
    return 0;
}
