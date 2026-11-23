#ifndef TEMPLE_COURSE_H
#define TEMPLE_COURSE_H

#include "recovered.h"

#define TR_OPENING_TILES 12

typedef struct {
    int index;
    int obstacle; /* 0 none, 1 jump tree, 3 trip tree, 2 slide tree */
    int coin_lane; /* -1, 0, 1, or 2 for no coins */
    unsigned coins_collected;
    bool hit;
} TRTile;

typedef struct {
    TRTile tiles[TR_OPENING_TILES];
    int corner_direction; /* -1 left, +1 right */
    bool corner_reached;
} TRCourse;

void tr_course_init(TRCourse *course, int seed);
void tr_course_step(TRCourse *course, TRRunner *runner, float previous_forward,
    float previous_lateral, float lateral);

#endif
