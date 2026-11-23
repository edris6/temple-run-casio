#include "course.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static TRCourse empty_course(void)
{
    TRCourse c;
    memset(&c, 0, sizeof(c));
    for (int i = 0; i < TR_OPENING_TILES; i++) c.tiles[i].coin_lane = 2;
    return c;
}

int main(void)
{
    TRCourse a, b;
    tr_course_init(&a, 1);
    tr_course_init(&b, 1);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    /* Seed 1 checked against the managed reference algorithm and host scheduler. */
    const int expected_obstacles[] = {0,0,0,0,0,0,0,3,0,0,0,0};
    const int expected_lanes[] = {2,-1,-1,0,2,2,0,2,-1,0,2,2};
    assert(a.corner_direction == -1);
    for (int i = 0; i < TR_OPENING_TILES; i++) {
        assert(a.tiles[i].obstacle == expected_obstacles[i]);
        assert(a.tiles[i].coin_lane == expected_lanes[i]);
    }
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
    tr_reset(&runner); runner.forward = 104;
    tr_course_step(&c, &runner, 102, 0, 0);
    assert(runner.dead && c.tiles[2].hit);
    c = empty_course(); c.tiles[2].obstacle = 1;
    tr_reset(&runner); runner.forward = 104; runner.height = 6;
    tr_course_step(&c, &runner, 102, 0, 0);
    assert(!runner.dead && !c.tiles[2].hit);

    c = empty_course(); c.tiles[2].obstacle = 2;
    tr_reset(&runner); runner.forward = 98;
    tr_course_step(&c, &runner, 96, 0, 0);
    assert(runner.dead);
    c = empty_course(); c.tiles[2].obstacle = 2;
    tr_reset(&runner); runner.forward = 98; runner.sliding = true;
    tr_course_step(&c, &runner, 96, 0, 0);
    assert(!runner.dead);

    c = empty_course(); c.tiles[2].obstacle = 3;
    tr_reset(&runner); runner.forward = 100; runner.stumble_kill_timer = 10;
    tr_course_step(&c, &runner, 98, 0, 0);
    assert(!runner.dead && runner.stumbling && runner.speed == 85);

    c = empty_course(); c.tiles[6].coin_lane = 0;
    tr_reset(&runner); runner.forward = 316;
    tr_course_step(&c, &runner, 315, 0, 0);
    assert(runner.coins == 1 && c.tiles[6].coins_collected == 1);
    tr_course_step(&c, &runner, 315, 0, 0);
    assert(runner.coins == 1);
    c = empty_course(); c.tiles[6].coin_lane = 1;
    tr_reset(&runner); runner.forward = 316;
    tr_course_step(&c, &runner, 315, 0, 0);
    assert(runner.coins == 0);

    c = empty_course(); tr_reset(&runner); runner.forward = 628;
    tr_course_step(&c, &runner, 627, 0, 0);
    assert(c.corner_reached);
    puts("Opening course checks passed");
    return 0;
}
