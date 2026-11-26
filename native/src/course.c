/* Course scheduler and turn rules translated from the Unity reconstruction,
 * TempleRunFlow.Track.cs and PathElement.cs, commit 65964ad. Its scheduler is
 * authored glue; the original game's generator has not been recovered. */
#include "course.h"
#include <limits.h>
#include <math.h>
#include <string.h>

/* Seeded System.Random: Microsoft .NET reference source, MIT license;
 * see native/THIRD_PARTY_NOTICES.md. */
static int wrap_sub(int a, int b)
{
    uint32_t value = (uint32_t)a - (uint32_t)b;
    return value <= INT32_MAX ? (int)value : -1 - (int)(UINT32_MAX - value);
}

static void random_init(TRRandom *r, int seed)
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

static int next(TRRandom *r, int min, int max)
{
    if (++r->next >= 56) r->next = 1;
    if (++r->nextp >= 56) r->nextp = 1;
    int value = wrap_sub(r->values[r->next], r->values[r->nextp]);
    if (value == INT_MAX) value--;
    if (value < 0) value += INT_MAX;
    r->values[r->next] = value;
    return min + (int)((double)value * (1.0 / INT_MAX) * (max-min));
}

static int dx(int direction) { return direction == 2 ? 1 : direction == 4 ? -1 : 0; }
static int dz(int direction) { return direction == 1 ? 1 : direction == 3 ? -1 : 0; }
static int turn_heading(int direction, int sign)
{ return sign < 0 ? (direction + 2) % 4 + 1 : direction % 4 + 1; }

/* Exact Region2D vertices from the four temple hole and narrow prefabs.
 * Coordinates are world-aligned in the reconstruction, even when the runner
 * travels south or west. */
bool tr_terrain_ground(const TRTile *tile, float x, float z)
{
    static const TRVec2 hole_v_a[] = {{10,-30},{10,-25},{-10,-25},{-10,-30}};
    static const TRVec2 hole_v_b[] = {{10,25},{10,30},{-10,30},{-10,25}};
    static const TRVec2 hole_h_a[] = {{30,-10},{30,10},{25,10},{25,-10}};
    static const TRVec2 hole_h_b[] = {{-25,-10},{-25,10},{-30,10},{-30,-10}};
    static const TRVec2 narrow_v[] = {
        {10,-30},{10,-28},{1,-25},{-1,0},{1,25},{10,28},{10,30},{-10,30},{-10,-30}};
    static const TRVec2 narrow_h[] = {
        {30,-10},{30,10},{-30,10},{-30,-10},{-28,-10},{-25,-1},{0,1},{25,-1},{28,-10}};
    TRVec2 p = {x-tile->x, z-tile->z};
    bool vertical = tile->incoming == 1 || tile->incoming == 3;
    if (tile->terrain == 1) {
        const TRVec2 *a = vertical ? hole_v_a : hole_h_a;
        const TRVec2 *b = vertical ? hole_v_b : hole_h_b;
        return tr_point_inside(a, 4, p) || tr_point_inside(b, 4, p);
    }
    if (tile->terrain == 2)
        return tr_point_inside(vertical ? narrow_v : narrow_h, 9, p);
    return false;
}

bool tr_course_ground_at(const TRCourse *c, float x, float z)
{
    static const TRVec2 straight_v[] = {{10,-30},{10,30},{-10,30},{-10,-30}};
    static const TRVec2 straight_h[] = {{30,-10},{30,10},{-30,10},{-30,-10}};
    static const TRVec2 corners[][6] = {
        {{-30,10},{-30,-10},{-10,-10},{-10,-30},{10,-30},{10,10}},
        {{30,10},{30,-10},{10,-10},{10,-30},{-10,-30},{-10,10}},
        {{-30,-10},{-30,10},{-10,10},{-10,30},{10,30},{10,-10}},
        {{30,-10},{30,10},{10,10},{10,30},{-10,30},{-10,-10}}
    };
    static const int left[] = {0,0,2,3,1}, right[] = {0,1,0,2,3};
    int first = c->current > 1 ? c->current - 2 : 0;
    int last = c->current + 4 < c->count ? c->current + 4 : c->count;
    for (int i = first; i < last; i++) {
        const TRTile *tile = &c->tiles[i];
        TRVec2 point = {x-tile->x, z-tile->z};
        if (point.x < -30 || point.x > 30 || point.y < -30 || point.y > 30) continue;
        if (tile->terrain) {
            if (tr_terrain_ground(tile, x, z)) return true;
        } else if (tile->turn) {
            int corner = tile->turn < 0 ? left[tile->incoming] : right[tile->incoming];
            if (tr_point_inside(corners[corner], 6, point)) return true;
            if (tile->junction) {
                corner = tile->turn < 0 ? right[tile->incoming] : left[tile->incoming];
                if (tr_point_inside(corners[corner], 6, point)) return true;
            }
        } else {
            bool vertical = tile->incoming == 1 || tile->incoming == 3;
            if (tr_point_inside(vertical ? straight_v : straight_h, 4, point)) return true;
        }
    }
    return false;
}

static void append_tile(TRCourse *c, float distance)
{
    TRTile *previous = c->count ? &c->tiles[c->count-1] : NULL;
    TRTile *tile = &c->tiles[c->count++];
    memset(tile, 0, sizeof(*tile));
    tile->index = c->generated++;
    tile->coin_lane = 2;
    tile->incoming = previous ? previous->outgoing : 1;
    tile->outgoing = tile->incoming;
    if (previous) {
        tile->x = previous->x + 60 * dx(previous->outgoing);
        tile->z = previous->z + 60 * dz(previous->outgoing);
    }
    if (--c->until_turn <= 0) {
        tile->turn = next(&c->random, 0, 2) == 0 ? -1 : 1;
        tile->junction = true;
        tile->outgoing = turn_heading(tile->incoming, tile->turn);
        c->until_turn = next(&c->random, distance < 250 ? 10 : distance < 1000 ? 6 : 3, 13);
    } else {
        c->since_obstacle++;
        if (tile->index > 3 && c->since_obstacle >= (distance < 250 ? 6 : 4) &&
            next(&c->random, 0, 100) < 60) {
            c->since_obstacle = 0;
            int hazard = next(&c->random, 0, distance < 250 ? 2 : distance < 500 ? 4 : 5);
            if (hazard == 0) tile->obstacle = 1;
            else if (hazard == 1) tile->obstacle = 3;
            else if (hazard == 2) tile->terrain = 1;
            else if (hazard == 3) tile->terrain = 2;
            else tile->obstacle = 2;
        }
        /* Introduce each requested obstacle after the first junction, with
         * clear tiles between them. Placement is authored for this port. */
        if (tile->index >= 12 && tile->index <= 20) {
            tile->obstacle = 0; tile->terrain = 0;
            if (tile->index == 14) tile->terrain = 2;
            if (tile->index == 17) tile->terrain = 1;
            if (tile->index == 20) tile->obstacle = 2;
        }
        if (tile->index > 0 && tile->index % 6 < 4 && tile->obstacle == 0)
            tile->coin_lane = next(&c->random, -1, 2);
        /* Default profile has no power-up upgrades. The probability draw is
         * still consumed by the reference host, even when none are available. */
        if (tile->obstacle == 0 && tile->index > 15 && c->elapsed >= 15.0f)
            (void)next(&c->random, 0, 100);
    }
}

void tr_course_init(TRCourse *c, int seed)
{
    memset(c, 0, sizeof(*c));
    random_init(&c->random, seed);
    c->until_turn = 12;
    c->heading = 1;
    c->view_z = 1;
    c->z = -15.0f;
    for (int i = 0; i < TR_COURSE_LOOKAHEAD; i++) append_tile(c, 0);
}

bool tr_course_turn_near(const TRCourse *c, const TRRunner *p)
{
    const TRTile *tile = &c->tiles[c->current];
    float ahead = (tile->x-c->x)*dx(tile->incoming) + (tile->z-c->z)*dz(tile->incoming);
    if (tile->turn && !tile->turned && ahead > -30.0f) return true;
    if (c->current + 1 >= c->count) return false;
    const TRTile *next_tile = &c->tiles[c->current+1];
    float next_ahead = (next_tile->x-c->x)*dx(c->heading) + (next_tile->z-c->z)*dz(c->heading);
    return next_tile->turn && next_ahead > 0 && next_ahead <= p->speed*0.4f+15.0f;
}

/* The generated continuation can be rotated 180 degrees at a junction. This
 * supplies either exit without storing two separate courses. */
static void choose_junction(TRCourse *c, TRTile *tile, int sign)
{
    if (sign != tile->turn) {
        for (int i = c->current + 1; i < c->count; i++) {
            TRTile *next_tile = &c->tiles[i];
            next_tile->x = 2*tile->x - next_tile->x;
            next_tile->z = 2*tile->z - next_tile->z;
            next_tile->incoming = (next_tile->incoming + 1) % 4 + 1;
            next_tile->outgoing = (next_tile->outgoing + 1) % 4 + 1;
        }
        tile->turn = sign;
        tile->outgoing = turn_heading(tile->incoming, sign);
    }
}

void tr_course_request_turn(TRCourse *c, TRRunner *p, float *lateral, int sign)
{
    if (p->dead || !sign) return;
    sign = sign < 0 ? -1 : 1;
    TRTile *tile = &c->tiles[c->current];
    float ahead = (tile->x-c->x)*dx(tile->incoming) + (tile->z-c->z)*dz(tile->incoming);
    if (tile->turn && !tile->turned && fabsf(ahead) <= 12.0f &&
        (tile->junction || sign == tile->turn)) {
        if (tile->junction) choose_junction(c, tile, sign);
        c->x = (float)tile->x; c->z = (float)tile->z;
        c->heading = tile->outgoing;
        *lateral = 0;
        tile->turned = true;
        c->buffered_turn = 0;
        return;
    }
    if (c->current + 1 < c->count) {
        TRTile *following = &c->tiles[c->current+1];
        float next_ahead = (following->x-c->x)*dx(c->heading) + (following->z-c->z)*dz(c->heading);
        if ((following->turn && next_ahead > 0 && next_ahead <= p->speed*0.4f+15.0f) ||
            (tile->turn && !tile->turned && ahead > 0)) {
            c->buffered_turn = sign;
            c->turn_buffer_time = 0.5f;
            return;
        }
    }
    tr_stumble(p);
}

void tr_course_process_turn(TRCourse *c, TRRunner *p, float *lateral, float dt)
{
    if (!c->buffered_turn || p->dead) return;
    TRTile *tile = &c->tiles[c->current];
    c->turn_buffer_time -= dt;
    float ahead = (tile->x-c->x)*dx(tile->incoming) + (tile->z-c->z)*dz(tile->incoming);
    if (tile->turn && !tile->turned && fabsf(ahead) <= 10.0f) {
        int sign = c->buffered_turn;
        c->buffered_turn = 0;
        tr_course_request_turn(c, p, lateral, sign);
    } else if (c->turn_buffer_time <= 0) c->buffered_turn = 0;
}

static float axis_distance(float p, float low, float high)
{ return p < low ? low-p : p > high ? p-high : 0; }

void tr_course_step(TRCourse *c, TRRunner *p, float previous_forward,
    float previous_lateral, float lateral)
{
    if (p->dead) return;
    float old_x = c->x, old_z = c->z;
    float displacement = p->forward - previous_forward, side = lateral - previous_lateral;
    c->x += displacement * dx(c->heading) + side * dz(c->heading);
    c->z += displacement * dz(c->heading) - side * dx(c->heading);
    c->elapsed += 1.0f/30.0f;
    int first = c->current > 0 ? c->current-1 : 0;
    int last = c->current+3 < c->count ? c->current+3 : c->count;
    for (int i = first; i < last && !p->dead; i++) {
        TRTile *tile = &c->tiles[i];
        if (fabsf(c->x-tile->x) > 40 || fabsf(c->z-tile->z) > 40) continue;
        if (tile->obstacle && !tile->hit) {
            int vertical = tile->incoming == 1 || tile->incoming == 3;
            float l, r, n, f;
            if (vertical) {
                l = -10; r = tile->obstacle == 3 ? 5 : 10;
                n = tile->obstacle == 1 ? 3 : tile->obstacle == 2 ? -3 : -1;
                f = tile->obstacle == 1 ? 7 : tile->obstacle == 2 ? 3 : 1;
            } else {
                l = tile->obstacle == 1 ? -7 : tile->obstacle == 2 ? -3 : -1;
                r = tile->obstacle == 1 ? -3 : tile->obstacle == 2 ? 3 : 1;
                n = tile->obstacle == 3 ? -5 : -10; f = 10;
            }
            TRVec2 points[] = {{l,n},{r,n},{r,f},{l,f}};
            TRVec2 p0 = {old_x-tile->x,old_z-tile->z}, p1 = {c->x-tile->x,c->z-tile->z};
            bool jump = tile->obstacle != 2, slide = tile->obstacle == 2;
            float height = tile->obstacle == 1 ? 6 : tile->obstacle == 2 ? 8 : 1;
            if (tr_enters_obstacle(points, 4, p0, p1, jump, slide, p->height, height, p->sliding)) {
                tile->hit = true;
                if (tile->obstacle == 3) tr_stumble(p); else p->dead = true;
            }
        }
        if (tile->coin_lane != 2 && !p->dead) for (int j = 0; j < 6; j++) {
            if (tile->coins_collected & (1u << j)) continue;
            float along = -24.0f + 9.0f*j, lane = 6.0f * tile->coin_lane;
            float coin_x = tile->x + along*dx(tile->incoming) + lane*dz(tile->incoming);
            float coin_z = tile->z + along*dz(tile->incoming) - lane*dx(tile->incoming);
            float x = axis_distance(coin_x, c->x-1.7469f, c->x+1.7469f);
            float y = axis_distance(3, p->height-0.16485f, p->height+9.58665f);
            float z = axis_distance(coin_z, c->z-1.7469f, c->z+1.7469f);
            if (x*x + y*y + z*z <= 2.25f*2.25f) {
                tile->coins_collected |= 1u << j;
                tr_add_coins(p, 1);
            }
        }
    }
    TRTile *current = &c->tiles[c->current];
    float past = (c->x-current->x)*dx(current->incoming) + (c->z-current->z)*dz(current->incoming);
    /* The reference falls off the unturned path; falling animation and exact
     * ground regions are still missing, so end the run at the tile edge. */
    if (current->turn && !current->turned && past > 30.0f) p->dead = true;
    while (c->current + 1 < c->count) {
        current = &c->tiles[c->current];
        if (current->turn && !current->turned) break;
        float beyond = (c->x-current->x)*dx(current->outgoing) + (c->z-current->z)*dz(current->outgoing);
        if (beyond <= 30.0f) break;
        c->current++;
    }
    if (c->current > 8) {
        int remove = c->current - 4;
        memmove(c->tiles, c->tiles+remove, (c->count-remove)*sizeof(c->tiles[0]));
        c->count -= remove; c->current -= remove;
    }
    while (c->count-c->current < TR_COURSE_LOOKAHEAD && c->count < TR_COURSE_CAPACITY)
        append_tile(c, p->distance);
}
