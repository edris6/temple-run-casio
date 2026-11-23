#include "session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static TRSession approaching_jump_tree(void)
{
    TRSession s;
    tr_session_reset(&s, 1);
    memset(&s.course, 0, sizeof(s.course));
    for (int i = 0; i < TR_OPENING_TILES; i++) s.course.tiles[i].coin_lane = 2;
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
    puts("Input and session checks passed");
    return 0;
}
