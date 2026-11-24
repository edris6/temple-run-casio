#include "session.h"
#include <assert.h>
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
    puts("Input and session checks passed");
    return 0;
}
