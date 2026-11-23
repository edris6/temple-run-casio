/* Opening straight of TempleRunFlow.Track.cs at 65964ad. The source
 * reconstruction's scheduler is authored glue, not original native code. */
#include "course.h"
#include <limits.h>
#include <math.h>
#include <string.h>

/* The seeded System.Random algorithm used by Unity's managed runtime.
 * Algorithm reference: Microsoft .NET reference source, MIT license:
 * https://github.com/microsoft/referencesource/blob/main/mscorlib/system/random.cs */
typedef struct { int values[56], next, nextp; } Random;

/* C# uses unchecked 32-bit integer arithmetic here. */
static int wrap_sub(int a, int b)
{
    uint32_t value = (uint32_t)a - (uint32_t)b;
    return value <= INT32_MAX ? (int)value : -1 - (int)(UINT32_MAX - value);
}

static void random_init(Random *r, int seed)
{
    memset(r, 0, sizeof(*r));
    int subtraction = seed == INT_MIN ? INT_MAX : seed < 0 ? -seed : seed;
    int mj = 161803398 - subtraction;
    r->values[55] = mj;
    int mk = 1;
    for (int i = 1; i < 55; i++) {
        int ii = (21*i)%55;
        r->values[ii] = mk;
        mk = wrap_sub(mj, mk);
        if (mk < 0) mk += INT_MAX;
        mj = r->values[ii];
    }
    for (int k = 1; k < 5; k++) for (int i = 1; i < 56; i++) {
        r->values[i] = wrap_sub(r->values[i], r->values[1+(i+30)%55]);
        if (r->values[i] < 0) r->values[i] += INT_MAX;
    }
    r->next = 0; r->nextp = 21;
}

static int next(Random *r, int min, int max)
{
    if (++r->next >= 56) r->next = 1;
    if (++r->nextp >= 56) r->nextp = 1;
    int value = wrap_sub(r->values[r->next], r->values[r->nextp]);
    if (value == INT_MAX) value--;
    if (value < 0) value += INT_MAX;
    r->values[r->next] = value;
    return min + (int)((double)value * (1.0 / INT_MAX) * (max-min));
}

void tr_course_init(TRCourse *course, int seed)
{
    memset(course, 0, sizeof(*course));
    Random random;
    random_init(&random, seed);
    int since_obstacle = 0, until_turn = 12;
    for (int i = 0; i < TR_OPENING_TILES; i++) {
        TRTile *tile = &course->tiles[i];
        tile->index = i;
        tile->coin_lane = 2;
        if (--until_turn <= 0) {
            course->corner_direction = next(&random, 0, 2) == 0 ? -1 : 1;
            /* Host also draws the next turn interval at generation time. */
            until_turn = next(&random, 10, 13);
            continue;
        }
        since_obstacle++;
        if (i > 3 && since_obstacle >= 6 && next(&random, 0, 100) < 60) {
            since_obstacle = 0;
            tile->obstacle = next(&random, 0, 2) == 0 ? 1 : 3;
        }
        if (i > 0 && i % 6 < 4 && tile->obstacle == 0)
            tile->coin_lane = next(&random, -1, 2);
    }
}

/* Distance from a point to the recovered player AABB in world coordinates. */
static float axis_distance(float p, float low, float high)
{ return p < low ? low-p : p > high ? p-high : 0; }

void tr_course_step(TRCourse *course, TRRunner *runner, float previous_forward,
    float previous_lateral, float lateral)
{
    if (runner->dead || course->corner_reached) return;
    /* The renderer places the runner 20 units in front of its camera anchor. */
    float before = previous_forward + 20.0f, current = runner->forward + 20.0f;
    for (int i = 0; i < TR_OPENING_TILES - 1 && !runner->dead; i++) {
        TRTile *tile = &course->tiles[i];
        float center = 60.0f * i;
        if (current < center-35.0f || before > center+35.0f) continue;
        if (tile->obstacle && !tile->hit) {
            TRVec2 points[4];
            float left = -10.0f;
            float right = tile->obstacle == 3 ? 5.0f : 10.0f;
            float near = tile->obstacle == 1 ? 3.0f : tile->obstacle == 2 ? -3.0f : -1.0f;
            float far = tile->obstacle == 1 ? 7.0f : tile->obstacle == 2 ? 3.0f : 1.0f;
            points[0] = (TRVec2){left,near}; points[1] = (TRVec2){right,near};
            points[2] = (TRVec2){right,far}; points[3] = (TRVec2){left,far};
            TRVec2 p0 = {previous_lateral, before-center}, p1 = {lateral, current-center};
            bool jump = tile->obstacle != 2, slide = tile->obstacle == 2;
            float height = tile->obstacle == 1 ? 6.0f : tile->obstacle == 2 ? 8.0f : 1.0f;
            if (tr_enters_obstacle(points, 4, p0, p1, jump, slide,
                                   runner->height, height, runner->sliding)) {
                tile->hit = true;
                if (tile->obstacle == 3) tr_stumble(runner);
                else runner->dead = true;
            }
        }
        if (tile->coin_lane != 2 && !runner->dead) for (int j = 0; j < 6; j++) {
            if (tile->coins_collected & (1u << j)) continue;
            float coin_x = 6.0f * tile->coin_lane;
            float coin_z = center - 24.0f + 9.0f*j;
            float dx = axis_distance(coin_x, lateral-1.7469f, lateral+1.7469f);
            float dy = axis_distance(3.0f, runner->height-0.16485f, runner->height+9.58665f);
            float dz = axis_distance(coin_z, current-1.7469f, current+1.7469f);
            if (dx*dx + dy*dy + dz*dz <= 2.25f*2.25f) {
                tile->coins_collected |= 1u << j;
                tr_add_coins(runner, 1);
            }
        }
    }
    /* Stop at the first corner until connected corner geometry is ported. */
    if (current >= 60.0f * (TR_OPENING_TILES - 1) - 12.0f)
        course->corner_reached = true;
}
