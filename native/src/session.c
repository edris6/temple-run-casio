#include "session.h"
#include <math.h>

typedef struct {
    const TRCourse *course;
    float previous_forward, side;
} GroundQuery;

static bool ground_at(float forward, void *context)
{
    const GroundQuery *query = context;
    const TRCourse *course = query->course;
    float along = forward - query->previous_forward;
    int dx = course->heading == 2 ? 1 : course->heading == 4 ? -1 : 0;
    int dz = course->heading == 1 ? 1 : course->heading == 3 ? -1 : 0;
    return tr_course_ground_at(course,
        course->x + along*dx + query->side*dz,
        course->z + along*dz - query->side*dx);
}

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
    if (input.toggle_pause && !runner->dead)
        session->paused = !session->paused;
    if (input.restart && runner->dead) {
        tr_session_reset(session, new_seed);
        ticks = 0; /* Time accumulated on the result screen is not gameplay. */
    }
    /* Apply input queued during rendering before simulating accumulated ticks. */
    if (input.jump && !session->paused) tr_jump(runner);
    if (input.slide && !session->paused) tr_slide(runner);
    if (input.turn && !session->paused) {
        if (tr_course_turn_near(course, runner)) {
            tr_course_request_turn(course, runner, &session->lateral, input.turn);
            input.tilt = 0;
        } else {
            /* The same arrow keys move sideways on a straight path. */
            input.tilt = input.turn;
        }
    }
    if (course->buffered_turn) input.tilt = 0;
    if (ticks > 30) ticks = 30;
    for (int i = 0; i < ticks; i++) {
        if (runner->dead || session->paused) break;
        tr_course_process_turn(course, runner, &session->lateral, 1.0f/30.0f);
        float previous_forward = runner->forward, previous_lateral = session->lateral;
        if (input.tilt < 0) session->lateral -= 0.6f;
        if (input.tilt > 0) session->lateral += 0.6f;
        if (session->lateral < -7) session->lateral = -7;
        if (session->lateral > 7) session->lateral = 7;
        GroundQuery ground = {course, previous_forward, session->lateral-previous_lateral};
        tr_step_with_ground(runner, 1.0f/30.0f, ground_at, &ground);
        tr_course_step(course, runner, previous_forward, previous_lateral, session->lateral);
        /* Approximate the reference host's smoothed camera turn. */
        float target_x = course->heading == 2 ? 1 : course->heading == 4 ? -1 : 0;
        float target_z = course->heading == 1 ? 1 : course->heading == 3 ? -1 : 0;
        course->view_x += (target_x-course->view_x) * 0.2834687f;
        course->view_z += (target_z-course->view_z) * 0.2834687f;
        float length = sqrtf(course->view_x*course->view_x + course->view_z*course->view_z);
        if (length > 0) { course->view_x /= length; course->view_z /= length; }
    }
}

void tr_session_pose(const TRSession *session, TRPose *pose)
{
    const TRCourse *course = &session->course;
    pose->x = course->x; pose->z = course->z;
    pose->view_x = course->view_x; pose->view_z = course->view_z;
    pose->height = session->runner.height;
}

void tr_session_predict(const TRSession *session, int tilt, TRPose *pose)
{
    static TRSession next; /* too large to spend on the calculator's stack */
    next = *session;
    tr_session_advance(&next, (TRInput){.tilt = tilt}, 1, 0);
    tr_session_pose(&next, pose);
}

void tr_pose_between(const TRPose *a, const TRPose *b, float t, TRPose *pose)
{
    pose->x = a->x + (b->x - a->x) * t;
    pose->z = a->z + (b->z - a->z) * t;
    pose->height = a->height + (b->height - a->height) * t;
    float x = a->view_x + (b->view_x - a->view_x) * t;
    float z = a->view_z + (b->view_z - a->view_z) * t;
    /* One Newton step towards unit length is enough between adjacent steps
     * and avoids a square root. */
    float scale = 1.5f - 0.5f * (x * x + z * z);
    pose->view_x = x * scale; pose->view_z = z * scale;
}
