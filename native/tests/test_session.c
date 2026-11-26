#include "session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static TRSession approaching_jump_tree(void)
{
    TRSession s;
    tr_session_reset(&s, 1);
    memset(&s.course, 0, sizeof(s.course));
    s.course.count = TR_OPENING_TILES; s.course.heading = 1; s.course.z = 114.9f;
    for (int i = 0; i < TR_OPENING_TILES; i++) {
        s.course.tiles[i].z = i*60; s.course.tiles[i].index = i;
        s.course.tiles[i].incoming = s.course.tiles[i].outgoing = 1;
        s.course.tiles[i].coin_lane = 2;
    }
    s.course.tiles[2].obstacle = 1;
    s.runner.forward = 94.9f; /* world z=114.9, tree begins at z=123 */
    return s;
}

int main(void)
{
    TRSession s = approaching_jump_tree();
    tr_session_advance(&s, (TRInput){0}, 3, 1);
    assert(s.runner.dead);
    s = approaching_jump_tree();
    tr_session_advance(&s, (TRInput){.jump=true}, 3, 1);
    assert(!s.runner.dead && s.runner.jumping && s.runner.height >= 6);
    assert(s.runner.forward + 20 > 123);

    tr_session_reset(&s, 1);
    float start = s.runner.forward;
    tr_session_advance(&s, (TRInput){.toggle_pause=true}, 15, 1);
    assert(s.paused && s.runner.forward == start);
    tr_session_advance(&s, (TRInput){.jump=true}, 15, 1);
    assert(s.paused && !s.runner.jumping && s.runner.forward == start);
    tr_session_advance(&s, (TRInput){.toggle_pause=true}, 1, 1);
    assert(!s.paused && s.runner.forward > start);
    s.runner.dead = true;
    tr_session_advance(&s, (TRInput){.restart=true}, 30, 1);
    assert(!s.runner.dead && s.runner.forward == -35 && s.runner.coins == 0);

    /* Restart is ignored during a run, and works from the pause menu. */
    tr_session_advance(&s, (TRInput){0}, 15, 1);
    float travelled = s.runner.forward;
    tr_session_advance(&s, (TRInput){.restart=true}, 1, 1);
    assert(s.runner.forward > travelled);
    tr_session_advance(&s, (TRInput){.toggle_pause=true}, 1, 1);
    tr_session_advance(&s, (TRInput){.restart=true, .toggle_pause=true}, 30, 1);
    assert(!s.paused && s.runner.forward == -35);

    tr_session_reset(&s, 1);
    s.course.current = 11; s.course.z = 650;
    s.runner.stumble_kill_timer = 10;
    tr_session_advance(&s, (TRInput){.turn=-1}, 1, 1);
    assert(s.course.tiles[s.course.current].index == 11 &&
           s.course.tiles[s.course.current].turned && s.course.heading == 4);
    assert(s.course.x < 0 && s.course.z == 660);
    assert(s.course.view_x < 0 && s.course.view_z > 0);

    tr_session_reset(&s, 1);
    tr_session_advance(&s, (TRInput){.turn=-1}, 3, 1);
    assert(!s.runner.dead && !s.runner.stumbling && s.lateral < 0);
    tr_session_advance(&s, (TRInput){.turn=1}, 3, 1);
    assert(!s.runner.dead && !s.runner.stumbling && s.lateral == 0);

    tr_session_reset(&s, 1);
    s.course.current = 10; s.course.z = 610;
    tr_session_advance(&s, (TRInput){.turn=-1, .tilt=-1}, 1, 1);
    assert(s.course.buffered_turn == -1 && s.lateral == 0);

    /* Use the imported gap regions for falling and landing after integration. */
    s = approaching_jump_tree();
    s.course.tiles[2].obstacle = 0; s.course.tiles[2].terrain = 1;
    s.course.current = 1; s.course.z = 90; s.runner.forward = 70;
    TRSession jump_gap = s;
    for (int i = 0; i < 30; i++)
        tr_session_advance(&jump_gap, (TRInput){.jump=i == 0}, 1, 1);
    assert(!jump_gap.runner.dead && jump_gap.course.z > 145 && jump_gap.runner.height == 0);
    for (int i = 0; i < 30; i++) tr_session_advance(&s, (TRInput){0}, 1, 1);
    assert(s.runner.dead && s.runner.falling);

    s = approaching_jump_tree();
    s.course.tiles[2].obstacle = 0; s.course.tiles[2].terrain = 2;
    s.course.current = 1; s.course.z = 90; s.course.x = -5; s.runner.forward = 70;
    for (int i = 0; i < 30; i++) tr_session_advance(&s, (TRInput){0}, 1, 1);
    assert(!s.runner.dead && s.course.z > 145);

    /* The same D-pad inputs used on the calculator must clear each hazard. */
    s = approaching_jump_tree();
    s.course.tiles[2].obstacle = 2;
    TRSession duck = s;
    tr_session_advance(&s, (TRInput){0}, 3, 1);
    assert(s.runner.dead);
    tr_session_advance(&duck, (TRInput){.slide=true}, 3, 1);
    assert(!duck.runner.dead && duck.runner.sliding && duck.course.z > 123);

    /* At maximum speed, finish a slide, have time to react, then jump a gap
     * at the minimum spacing used by the generator. */
    s = approaching_jump_tree();
    s.course.current = 1; s.course.tiles[2].obstacle = 2;
    s.course.tiles[8].terrain = 1;
    s.runner.speed = 210;
    tr_session_advance(&s, (TRInput){.slide=true}, 1, 1);
    tr_session_advance(&s, (TRInput){0}, 23, 1);
    assert(!s.runner.dead && !s.runner.sliding && s.course.z < 287);
    /* The gap starts at z=455, leaving at least 0.8 seconds after sliding. */
    assert((455 - s.course.z) / s.runner.speed >= 0.8f);
    tr_session_advance(&s, (TRInput){0}, 16, 1);
    tr_session_advance(&s, (TRInput){.jump=true}, 1, 1);
    tr_session_advance(&s, (TRInput){0}, 20, 1);
    assert(!s.runner.dead && s.course.z > 505);

    s = approaching_jump_tree();
    s.course.tiles[2].obstacle = 0; s.course.tiles[2].terrain = 2;
    s.course.current = 1; s.course.z = 85; s.runner.forward = 65;
    tr_session_advance(&s, (TRInput){.turn=-1}, 10, 1);
    tr_session_advance(&s, (TRInput){0}, 25, 1);
    assert(!s.runner.dead && s.course.z > 145 && s.lateral < 0);

    for (int sign = -1; sign <= 1; sign += 2) {
        tr_session_reset(&s, 1);
        s.course.current = 11; s.course.z = 650;
        tr_session_advance(&s, (TRInput){.turn=sign}, 1, 1);
        assert(!s.runner.dead && s.course.tiles[s.course.current].index == 11 &&
               s.course.tiles[s.course.current].turned);
        assert(s.course.heading == (sign < 0 ? 4 : 2));
    }
    /* A predicted pose is exactly where the next step lands, and predicting
     * leaves the session alone. */
    tr_session_reset(&s, 1);
    s.runner.stumble_kill_timer = 10;
    for (int i = 0; i < 400; i++) {
        int tilt = i % 90 < 10 ? -1 : 0;
        TRSession before = s;
        TRPose predicted, landed;
        tr_session_predict(&s, tilt, &predicted);
        assert(!memcmp(&before, &s, sizeof s));
        tr_session_advance(&s, (TRInput){.tilt = tilt}, 1, 1);
        tr_session_pose(&s, &landed);
        assert(!memcmp(&predicted, &landed, sizeof landed));
        if (i % 40 == 0) tr_jump(&s.runner);
    }
    /* While paused or dead there is nothing to move towards. */
    TRPose now, then;
    s.paused = true;
    tr_session_pose(&s, &now); tr_session_predict(&s, 1, &then);
    assert(!memcmp(&now, &then, sizeof now));

    /* Blended poses meet both ends and keep the camera direction near unit
     * length through a turn. */
    TRPose from = {10, 20, 0, 1, 0}, to = {16, 28, -0.4692f, 0.8831f, 4}, mid;
    tr_pose_between(&from, &to, 0, &mid);
    assert(!memcmp(&mid, &from, sizeof mid));
    tr_pose_between(&from, &to, 1, &mid);
    assert(mid.x == to.x && mid.z == to.z && mid.height == to.height);
    assert(fabsf(mid.view_x - to.view_x) < 0.001f && fabsf(mid.view_z - to.view_z) < 0.001f);
    tr_pose_between(&from, &to, 0.5f, &mid);
    assert(mid.x == 13 && mid.z == 24 && mid.height == 2);
    assert(fabsf(mid.view_x*mid.view_x + mid.view_z*mid.view_z - 1) < 0.01f);
    puts("Input and session checks passed");
    return 0;
}
