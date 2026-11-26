#ifndef TEMPLE_COURSE_H
#define TEMPLE_COURSE_H

#include "recovered.h"

#define TR_OPENING_TILES 12
#define TR_COURSE_CAPACITY 32
#define TR_COURSE_LOOKAHEAD 16

typedef struct {
    int index;
    int x, z; /* tile center in world coordinates */
    int incoming, outgoing; /* 1 north, 2 east, 3 south, 4 west */
    int turn; /* -1 left, +1 right, 0 straight */
    int terrain; /* 0 ordinary, 1 hole, 2 narrow */
    int obstacle; /* 0 none, 1 jump tree, 3 trip tree, 2 slide tree */
    int coin_lane; /* -1, 0, 1, or 2 for no coins */
    unsigned coins_collected;
    bool hit;
    bool turned;
    bool junction; /* both left and right are available */
} TRTile;

typedef struct { int values[56], next, nextp; } TRRandom;

typedef struct {
    TRTile tiles[TR_COURSE_CAPACITY];
    int count, current, generated, until_turn, since_obstacle;
    TRRandom random;
    float x, z; /* actual player position */
    int heading;
    float view_x, view_z; /* smoothed camera facing direction */
    int buffered_turn;
    float turn_buffer_time, elapsed;
} TRCourse;

void tr_course_init(TRCourse *course, int seed);
bool tr_terrain_ground(const TRTile *tile, float x, float z);
bool tr_course_ground_at(const TRCourse *course, float x, float z);
bool tr_course_turn_near(const TRCourse *course, const TRRunner *runner);
void tr_course_request_turn(TRCourse *course, TRRunner *runner, float *lateral, int sign);
void tr_course_process_turn(TRCourse *course, TRRunner *runner, float *lateral, float dt);
void tr_course_step(TRCourse *course, TRRunner *runner, float previous_forward,
    float previous_lateral, float lateral);

#endif
