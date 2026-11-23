#include "recovered.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int near(float a, float b) { return fabsf(a - b) < 0.0001f; }

int main(void)
{
    TRRunner p;
    tr_reset(&p);
    assert(near(p.speed, 100) && p.multiplier == 10 && p.bonus_threshold == 100);
    assert(tr_jump(&p) && near(p.vertical_velocity, 100) && !tr_jump(&p));
    tr_step(&p, 1.0f/60.0f, true, 0);
    assert(p.jumping && p.height > 0 && near(p.speed, 100));
    for (int i = 0; i < 60; ++i) tr_step(&p, 1.0f/60.0f, true, 0);
    assert(!p.jumping && near(p.height, 0) && p.distance > 0 && p.score > 0);
    assert(tr_slide(&p) && !tr_slide(&p));
    float duration = p.slide_duration;
    assert(near(duration, fminf(p.speed * 0.007f, 0.75f)));
    for (int i = 0; i < 60; ++i) tr_step(&p, 1.0f/60.0f, true, 0);
    assert(!p.sliding);
    tr_reset(&p);
    tr_add_coins(&p, 99);
    assert(p.score == 0 && p.coins_for_bonus == 99);
    tr_add_coins(&p, 1);
    assert(p.score == 5000 && p.coins_for_bonus == 0 && p.bonus_level == 2);
    tr_reset(&p);
    tr_stumble(&p);
    assert(p.dead); /* reset sets the native stumble timer to seven seconds */
    tr_reset(&p);
    p.stumble_kill_timer = 10;
    tr_stumble(&p);
    assert(!p.dead && p.stumbling && near(p.speed, 85));
    tr_stumble(&p);
    assert(p.dead);
    assert(tr_distance_increment(10, 0) == 0);
    assert(near(tr_distance_increment(100.0f/60.0f, 1.0f/60.0f), 5.0f/36.0f));

    const TRVec2 square[] = {{0,0},{2,0},{2,2},{0,2}};
    TRVec2 before = {-1,1}, after = {3,1}, inside = {1,1}, away = {-1,3};
    assert(tr_point_inside(square, 4, inside));
    assert(!tr_point_inside(square, 4, before));
    assert(tr_segment_intersects(square, 4, before, after));
    assert(!tr_segment_intersects(square, 4, before, away));
    assert(tr_enters_obstacle(square, 4, before, after, true, false, 1, 2, false));
    assert(!tr_enters_obstacle(square, 4, before, after, true, false, 2, 2, false));
    assert(!tr_enters_obstacle(square, 4, before, after, false, true, 0, 2, true));
    assert(!tr_enters_obstacle(square, 4, inside, after, false, false, 0, 2, false));
    puts("Recovered gameplay checks passed");
    return 0;
}
