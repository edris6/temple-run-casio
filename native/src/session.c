#include "session.h"

void tr_session_reset(TRSession *session, int seed)
{
    tr_reset(&session->runner);
    /* Player starts at z=-15; this renderer's camera anchor is 20 behind. */
    session->runner.forward = -35.0f;
    tr_course_init(&session->course, seed);
    session->lateral = 0;
    session->paused = false;
}

void tr_session_advance(TRSession *session, TRInput input, int ticks, int new_seed)
{
    TRRunner *runner = &session->runner;
    TRCourse *course = &session->course;
    if (input.toggle_pause && !runner->dead && !course->corner_reached)
        session->paused = !session->paused;
    if (input.restart && (runner->dead || course->corner_reached)) {
        tr_session_reset(session, new_seed);
        ticks = 0; /* Time accumulated on the result screen is not gameplay. */
    }
    /* Apply input queued during rendering before simulating accumulated ticks. */
    if (input.jump && !course->corner_reached && !session->paused) tr_jump(runner);
    if (input.slide && !course->corner_reached && !session->paused) tr_slide(runner);
    if (ticks > 30) ticks = 30;
    for (int i = 0; i < ticks; i++) {
        if (course->corner_reached || runner->dead || session->paused) break;
        float previous_forward = runner->forward, previous_lateral = session->lateral;
        if (input.tilt < 0) session->lateral -= 0.6f;
        if (input.tilt > 0) session->lateral += 0.6f;
        if (session->lateral < -7) session->lateral = -7;
        if (session->lateral > 7) session->lateral = 7;
        tr_step(runner, 1.0f/30.0f, true, 0);
        tr_course_step(course, runner, previous_forward, previous_lateral, session->lateral);
    }
}
