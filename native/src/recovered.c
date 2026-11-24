/* C translation of selected recovered Unity gameplay, commit 65964ad.
 * Source mappings and limitations: native/README.md. */
#include "recovered.h"
#include <math.h>
#include <string.h>

static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }

void tr_reset(TRRunner *p)
{
    memset(p, 0, sizeof(*p));
    p->speed = 100.0f;
    p->multiplier = 10;
    p->bonus_level = 1;
    p->bonus_threshold = 100;
    p->stumble_kill_timer = 7.0f;
}

bool tr_jump(TRRunner *p)
{
    if (p->jumping || p->stumbling || p->falling || p->dead) return false;
    p->vertical_velocity = minf(maxf(p->speed, 80.0f), 100.0f);
    p->jumping = true;
    p->sliding = false;
    return true;
}

bool tr_slide(TRRunner *p)
{
    if (p->sliding || p->jumping || p->falling || p->dead) return false;
    p->sliding = true;
    p->slide_time = 0.0f;
    p->slide_duration = minf(p->speed * 0.007f, 0.75f);
    return true;
}

void tr_stumble(TRRunner *p)
{
    if (p->dead) return;
    if (p->stumble_kill_timer < 10.0f) { p->dead = true; return; }
    p->speed *= 0.85f;
    p->stumbling = true;
    p->stumble_time = 0.0f;
    p->bonus_level = 1;
    p->coins_for_bonus = 0;
    p->stumble_duration = minf(p->speed * 0.005f, 0.5f);
    p->stumble_kill_timer = 0.0f;
}

float tr_distance_increment(float displacement, float dt)
{
    if (dt <= 0.0f) return 0.0f;
    float scaled_speed = displacement / dt / 60.0f;
    return dt * 60.0f * scaled_speed * scaled_speed / 20.0f;
}

static void step(TRRunner *p, float dt, bool over_ground, float ground_height,
    bool (*ground_at)(float forward, void *context), void *context)
{
    if (p->dead || dt <= 0.0f) return;
    float acceleration = 0.0f;
    if (!p->jumping && !p->sliding && p->speed < 210.0f) {
        float ratio = p->speed / 210.0f;
        acceleration = ratio < 0.50f ? 30.0f : ratio <= 0.65f ? 10.0f :
            ratio <= 0.85f ? 1.5f : 0.5f;
    }
    p->speed += acceleration * dt;
    p->vertical_velocity -= 250.0f * dt;
    float displacement = p->speed * minf(dt, 1.0f / 15.0f);
    p->forward += displacement;
    p->height += p->vertical_velocity * minf(dt, 1.0f / 15.0f);
    if (ground_at) over_ground = ground_at(p->forward, context);
    bool landed = over_ground && p->height <= ground_height &&
        (!p->falling || p->height >= ground_height - 6.0f);
    if (landed) {
        if (p->falling && p->height < ground_height - 2.0f) tr_stumble(p);
        p->height = ground_height;
        p->vertical_velocity = 0.0f;
        p->jumping = false;
    }
    p->falling = p->height < ground_height;
    if (p->sliding && (p->slide_time += dt) > p->slide_duration) p->sliding = false;
    if (p->stumbling && (p->stumble_time += dt) > p->stumble_duration) p->stumbling = false;
    p->stumble_kill_timer += dt;
    if (p->height < -35.0f) p->dead = true;
    if (!p->falling) {
        float increment = tr_distance_increment(displacement, dt);
        p->distance += increment;
        p->distance_remainder += increment;
        int whole = (int)p->distance_remainder;
        p->distance_remainder -= whole;
        /* Preserve unchecked 32-bit score arithmetic without signed overflow. */
        p->score = (int32_t)((uint32_t)p->score + (uint32_t)p->multiplier * (uint32_t)whole);
    }
}

void tr_step(TRRunner *p, float dt, bool over_ground, float ground_height)
{
    step(p, dt, over_ground, ground_height, NULL, NULL);
}

void tr_step_with_ground(TRRunner *p, float dt,
    bool (*ground_at)(float forward, void *context), void *context)
{
    step(p, dt, false, 0, ground_at, context);
}

void tr_add_coins(TRRunner *p, int count)
{
    if (count <= 0) return;
    p->coins += count;
    p->coins_for_bonus += count;
    if (p->coins_for_bonus >= p->bonus_threshold) {
        p->score = (int32_t)((uint32_t)p->score + (uint32_t)p->multiplier * 500u);
        p->bonus_level++;
        p->coins_for_bonus -= p->bonus_threshold;
    }
}

bool tr_point_inside(const TRVec2 *points, size_t count, TRVec2 p)
{
    if (!points || count < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        TRVec2 a = points[i], b = points[j];
        if (((a.y < p.y && p.y <= b.y) || (b.y < p.y && p.y <= a.y)) &&
            (a.x <= p.x || b.x < p.x) &&
            a.x + (b.x - a.x) * (p.y - a.y) / (b.y - a.y) < p.x) inside = !inside;
    }
    return inside;
}

static float cross(TRVec2 a, TRVec2 b) { return a.x * b.y - a.y * b.x; }
static TRVec2 sub(TRVec2 a, TRVec2 b) { TRVec2 r = {a.x-b.x, a.y-b.y}; return r; }

bool tr_segment_intersects(const TRVec2 *points, size_t count, TRVec2 a, TRVec2 b)
{
    if (!points || count < 3) return false;
    if (tr_point_inside(points, count, a) || tr_point_inside(points, count, b)) return true;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        TRVec2 c = points[j], d = points[i], r = sub(b,a), s = sub(d,c);
        float den = cross(r,s);
        if (fabsf(den) < 0.000001f) continue;
        TRVec2 q = sub(c,a);
        float t = cross(q,s)/den, u = cross(q,r)/den;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) return true;
    }
    return false;
}

bool tr_enters_obstacle(const TRVec2 *points, size_t count, TRVec2 previous,
    TRVec2 current, bool must_jump, bool must_slide, float height,
    float obstacle_height, bool sliding)
{
    if (tr_point_inside(points, count, previous)) return false;
    if (must_jump && height >= obstacle_height) return false;
    if (must_slide && sliding) return false;
    return tr_segment_intersects(points, count, previous, current);
}
