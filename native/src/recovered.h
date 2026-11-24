#ifndef TEMPLE_RECOVERED_H
#define TEMPLE_RECOVERED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { float x, y; } TRVec2;
typedef struct {
    float speed, height, vertical_velocity, forward;
    float slide_time, slide_duration, stumble_time, stumble_duration;
    float stumble_kill_timer, distance, distance_remainder;
    int32_t score, multiplier, coins, coins_for_bonus, bonus_threshold, bonus_level;
    bool jumping, sliding, stumbling, falling, dead;
} TRRunner;

void tr_reset(TRRunner *p);
bool tr_jump(TRRunner *p);
bool tr_slide(TRRunner *p);
void tr_stumble(TRRunner *p);
void tr_step(TRRunner *p, float dt, bool over_ground, float ground_height);
void tr_step_with_ground(TRRunner *p, float dt,
    bool (*ground_at)(float forward, void *context), void *context);
void tr_add_coins(TRRunner *p, int count);
float tr_distance_increment(float displacement, float dt);
bool tr_point_inside(const TRVec2 *points, size_t count, TRVec2 point);
bool tr_segment_intersects(const TRVec2 *points, size_t count, TRVec2 a, TRVec2 b);
bool tr_enters_obstacle(const TRVec2 *points, size_t count, TRVec2 previous,
    TRVec2 current, bool must_jump, bool must_slide, float height,
    float obstacle_height, bool sliding);

#endif
