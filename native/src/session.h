#ifndef TEMPLE_SESSION_H
#define TEMPLE_SESSION_H

#include "course.h"

typedef struct {
    TRRunner runner;
    TRCourse course;
    float lateral;
    bool paused;
} TRSession;

typedef struct {
    bool jump, slide, restart, toggle_pause;
    int tilt; /* -1 left, 0 neutral, +1 right */
    int turn; /* -1 left, 0 none, +1 right */
} TRInput;

void tr_session_reset(TRSession *session, int seed);
void tr_session_advance(TRSession *session, TRInput input, int ticks, int new_seed);
/* The camera and runner as simulated so far. */
void tr_session_pose(const TRSession *session, TRPose *pose);
/* The pose one step from now if the held tilt continues and nothing else is
 * pressed. The session itself is unchanged. */
void tr_session_predict(const TRSession *session, int tilt, TRPose *pose);
/* The pose a fraction t (0..1) of the way from a to b. */
void tr_pose_between(const TRPose *a, const TRPose *b, float t, TRPose *pose);

#endif
