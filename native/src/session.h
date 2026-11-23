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

#endif
