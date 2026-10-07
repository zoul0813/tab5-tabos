#include <pool/physics.h>
#include <stddef.h>

/* Floor square root, at most 32 iterations for any uint64_t input. */
static uint32_t square_root(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit    = UINT64_C(1) << 62U;
    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0U) {
        if (value >= result + bit) {
            value  -= result + bit;
            result  = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return (uint32_t) result;
}

static void constrain_axis(int32_t* position, int32_t* velocity, int32_t* remainder, int32_t high)
{
    const int32_t low = POOL_BALL_RADIUS * POOL_ONE;
    if (*position <= low) {
        *position = low;
        if (*velocity < 0) {
            *velocity  = (int32_t) (-(int64_t) *velocity * 85 / 100);
            *remainder = 0;
        }
    } else if (*position >= high) {
        *position = high;
        if (*velocity > 0) {
            *velocity  = (int32_t) (-(int64_t) *velocity * 85 / 100);
            *remainder = 0;
        }
    }
}

static void advance_axis(int32_t* position, int32_t* velocity, int32_t* remainder, int32_t high)
{
    const int32_t displacement  = *velocity + *remainder;
    *position                  += displacement / POOL_SUBSTEPS;
    *remainder                  = displacement % POOL_SUBSTEPS;
    constrain_axis(position, velocity, remainder, high);
}

static uint64_t speed_squared(pool_vec_t velocity)
{
    return (uint64_t) ((int64_t) velocity.x * velocity.x + (int64_t) velocity.y * velocity.y);
}

static void rolling_loss(pool_ball_t* ball)
{
    pool_vec_t* velocity = &ball->velocity;
    const uint32_t after = square_root(speed_squared(*velocity));
    if (after <= POOL_STOP_SPEED + POOL_FRICTION) {
        *velocity = (pool_vec_t) {0, 0};
    } else {
        velocity->x = (int32_t) ((int64_t) velocity->x * (after - POOL_FRICTION) / after);
        velocity->y = (int32_t) ((int64_t) velocity->y * (after - POOL_FRICTION) / after);
    }
}

static void cushions(pool_ball_t* ball, pool_vec_t* remainder)
{
    constrain_axis(&ball->position.x, &ball->velocity.x, &remainder->x,
                   (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE);
    constrain_axis(&ball->position.y, &ball->velocity.y, &remainder->y,
                   (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE);
}

enum {
    DIAMETER       = 2 * POOL_BALL_RADIUS * POOL_ONE,
    TIME_ONE       = 65536,
    CONTACT_PASSES = 4
};

static pool_vec_t difference(pool_vec_t a, pool_vec_t b)
{
    return (pool_vec_t) {a.x - b.x, a.y - b.y};
}

static pool_vec_t interpolate(pool_vec_t start, pool_vec_t end, int32_t time)
{
    return (pool_vec_t) {start.x + (int32_t) ((int64_t) (end.x - start.x) * time / TIME_ONE),
                         start.y + (int32_t) ((int64_t) (end.y - start.y) * time / TIME_ONE)};
}

/* Segment/circle entry, with a closest-point rejection and 16 bounded binary
 * refinements. Avoids squaring a quadratic discriminant on RV32. */
static int32_t circle_time(pool_vec_t start, pool_vec_t end, int32_t radius)
{
    const int64_t radius2 = (int64_t) radius * radius;
    if (speed_squared(start) <= (uint64_t) radius2) {
        return 0;
    }
    const pool_vec_t delta = difference(end, start);
    const int64_t length2  = (int64_t) speed_squared(delta);
    const int64_t dot      = (int64_t) start.x * delta.x + (int64_t) start.y * delta.y;
    if (length2 == 0 || dot >= 0) {
        return -1;
    }
    int64_t closest = -dot * TIME_ONE / length2;
    if (closest > TIME_ONE) {
        closest = TIME_ONE;
    }
    if (speed_squared(interpolate(start, end, (int32_t) closest)) > (uint64_t) radius2) {
        return -1;
    }
    int32_t low = 0, high = (int32_t) closest;
    for (unsigned int i = 0U; i < 16U && low + 1 < high; ++i) {
        const int32_t middle = low + (high - low) / 2;
        if (speed_squared(interpolate(start, end, middle)) <= (uint64_t) radius2) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return high;
}

static int64_t dot(pool_vec_t a, pool_vec_t b)
{
    return (int64_t) a.x * b.x + (int64_t) a.y * b.y;
}

static pool_vec_t point(pool_point_t p)
{
    return (pool_vec_t) {p.x * POOL_ONE, p.y * POOL_ONE};
}

/* A center crossing the finite throat into the sink is captured. The inward
 * half-plane is defined by the actual pocket center, not a screen-edge test. */
static int32_t mouth_time(pool_vec_t start, pool_vec_t end, const pool_pocket_t* pocket)
{
    const pool_vec_t a    = point(pocket->mouth_a);
    const pool_vec_t edge = difference(point(pocket->mouth_b), a);
    pool_vec_t normal     = {-edge.y, edge.x};
    if (dot(difference(point(pocket->center), a), normal) > 0) {
        normal.x = -normal.x;
        normal.y = -normal.y;
    }
    const int64_t from = dot(difference(start, a), normal);
    const int64_t to   = dot(difference(end, a), normal);
    if (to > 0) {
        return -1;
    }
    const int32_t time  = from > 0 ? (int32_t) (from * TIME_ONE / (from - to)) : 0;
    const pool_vec_t at = interpolate(start, end, time);
    const int64_t along = dot(difference(at, a), edge);
    if (along < 0 || along > (int64_t) speed_squared(edge)) {
        return -1;
    }
    return time;
}

static void reflect_jaw(pool_ball_t* ball, pool_vec_t normal)
{
    const int64_t squared  = (int64_t) speed_squared(normal);
    const int64_t approach = dot(ball->velocity, normal);
    if (approach >= 0 || squared == 0) {
        return;
    }
    const pool_vec_t reflected = {ball->velocity.x - (int32_t) (approach * normal.x * 185 / (squared * 100)),
                                  ball->velocity.y - (int32_t) (approach * normal.y * 185 / (squared * 100))};
    if (speed_squared(reflected) <= speed_squared(ball->velocity)) {
        ball->velocity = reflected;
    }
}

/* At most four chronological boundary events per substep. On exhaustion keep
 * the last corrected point, rather than advancing through an unchecked rail. */
static int open_boundary(pool_ball_t* ball, pool_vec_t start, pool_vec_t* remainder, pool_contact_state_t* contacts)
{
    pool_vec_t end       = ball->position;
    const int32_t margin = (POOL_BALL_RADIUS + POOL_JAW_RADIUS + 1) * POOL_ONE;
    if (start.x > margin && start.y > margin && end.x > margin && end.y > margin &&
        start.x < POOL_TABLE_WIDTH * POOL_ONE - margin && end.x < POOL_TABLE_WIDTH * POOL_ONE - margin &&
        start.y < POOL_TABLE_HEIGHT * POOL_ONE - margin && end.y < POOL_TABLE_HEIGHT * POOL_ONE - margin) {
        return -1;
    }
    int32_t remaining = (start.x == end.x && start.y == end.y) ? 0 : TIME_ONE;
    for (unsigned int event = 0U; event < 4U; ++event) {
        int32_t earliest = TIME_ONE + 1;
        int face = -1, jaw = -1, sink = -1;
        for (unsigned int i = 0U; i < POOL_CUSHION_COUNT; ++i) {
            const pool_cushion_t* c = &pool_cushions[i];
            const bool horizontal   = c->a.y == c->b.y;
            const int32_t low       = (horizontal ? c->a.x : c->a.y) * POOL_ONE;
            const int32_t high      = (horizontal ? c->b.x : c->b.y) * POOL_ONE;
            const int32_t sign      = (horizontal ? c->a.y : c->a.x) == 0 ? 1 : -1;
            const int32_t edge      = (horizontal ? c->a.y : c->a.x) * POOL_ONE;
            const int32_t from      = sign * ((horizontal ? start.y : start.x) - edge) - POOL_BALL_RADIUS * POOL_ONE;
            const int32_t to        = sign * ((horizontal ? end.y : end.x) - edge) - POOL_BALL_RADIUS * POOL_ONE;
            const int32_t velocity  = sign * (horizontal ? ball->velocity.y : ball->velocity.x);
            if (to <= 0 && (from > 0 || from < 0 || velocity < 0)) {
                const int32_t time  = from > 0 ? (int32_t) ((int64_t) from * TIME_ONE / (from - to)) : 0;
                const pool_vec_t at = interpolate(start, end, time);
                const int32_t along = horizontal ? at.x : at.y;
                if (along >= low && along <= high && time < earliest) {
                    earliest = time;
                    face     = (int) i;
                    jaw      = -1;
                }
            }
            for (unsigned int j = 0U; j < 2U; ++j) {
                const pool_vec_t center   = point(j == 0U ? c->a : c->b);
                const pool_vec_t from_jaw = difference(start, center);
                const int32_t radius      = (POOL_BALL_RADIUS + POOL_JAW_RADIUS) * POOL_ONE;
                if (speed_squared(from_jaw) >= (uint64_t) radius * (uint64_t) radius &&
                    dot(from_jaw, difference(end, start)) >= 0) {
                    continue;
                }
                const int32_t time = circle_time(from_jaw, difference(end, center), radius);
                if (time >= 0 && time < earliest) {
                    earliest = time;
                    jaw      = (int) (i * 2U + j);
                    face     = -1;
                }
            }
        }
        /* Boundary wins an exact tie with the mouth. */
        for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
            const int32_t time = mouth_time(start, end, &pool_pockets[i]);
            if (time >= 0 && time < earliest) {
                earliest = time;
                sink     = (int) i;
            }
        }
        if (earliest > TIME_ONE) {
            ball->position = end;
            return -1;
        }
        ball->position = interpolate(start, end, earliest);
        if (sink >= 0) {
            *remainder     = (pool_vec_t) {0, 0};
            ball->velocity = (pool_vec_t) {0, 0};
            return sink;
        }
        if (face >= 0) {
            const pool_cushion_t* c = &pool_cushions[face];
            const bool horizontal   = c->a.y == c->b.y;
            const int32_t sign      = (horizontal ? c->a.y : c->a.x) == 0 ? 1 : -1;
            int32_t* position       = horizontal ? &ball->position.y : &ball->position.x;
            int32_t* velocity       = horizontal ? &ball->velocity.y : &ball->velocity.x;
            *position               = (horizontal ? c->a.y : c->a.x) * POOL_ONE + sign * POOL_BALL_RADIUS * POOL_ONE;
            if (*velocity * sign < 0) {
                if (contacts != NULL && contacts->first != 0U) {
                    contacts->rail_after = true;
                }
                *velocity = (int32_t) (-(int64_t) *velocity * 85 / 100);
                if (horizontal) {
                    remainder->y = 0;
                } else {
                    remainder->x = 0;
                }
            }
            /* Preserve Stage 3's clamp-and-discard overshoot on straight rails:
             * leave tangential integration intact; no remaining normal travel. */
            if (horizontal) {
                ball->position.x = end.x;
            } else {
                ball->position.y = end.y;
            }
            return -1;
        }
        *remainder              = (pool_vec_t) {0, 0};
        const pool_cushion_t* c = &pool_cushions[(unsigned int) jaw / 2U];
        const pool_vec_t center = point((jaw & 1) == 0 ? c->a : c->b);
        pool_vec_t normal       = difference(ball->position, center);
        uint32_t distance       = square_root(speed_squared(normal));
        if (distance == 0U) {
            normal   = (pool_vec_t) {0, POOL_ONE};
            distance = POOL_ONE;
        }
        const pool_vec_t before_reflection = ball->velocity;
        reflect_jaw(ball, normal);
        if (contacts != NULL && contacts->first != 0U &&
            (before_reflection.x != ball->velocity.x || before_reflection.y != ball->velocity.y)) {
            contacts->rail_after = true;
        }
        const int32_t radius = (POOL_BALL_RADIUS + POOL_JAW_RADIUS) * POOL_ONE + 2;
        ball->position       = (pool_vec_t) {center.x + (int32_t) ((int64_t) normal.x * radius / distance),
                                             center.y + (int32_t) ((int64_t) normal.y * radius / distance)};
        remaining            = (int32_t) ((int64_t) remaining * (TIME_ONE - earliest) / TIME_ONE);
        start                = ball->position;
        end = (pool_vec_t) {start.x + (int32_t) ((int64_t) ball->velocity.x * remaining / (TIME_ONE * POOL_SUBSTEPS)),
                            start.y + (int32_t) ((int64_t) ball->velocity.y * remaining / (TIME_ONE * POOL_SUBSTEPS))};
    }
    return -1;
}

bool pool_physics_placement_valid(const pool_ball_t* balls, uint16_t potted, pool_vec_t position)
{
    if (position.x < POOL_BALL_RADIUS * POOL_ONE || position.y < POOL_BALL_RADIUS * POOL_ONE ||
        position.x > (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE ||
        position.y > (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE) {
        return false;
    }
    for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
        if ((potted & (1U << i)) == 0U &&
            speed_squared(difference(position, balls[i].position)) < (uint64_t) (DIAMETER + 16) * (DIAMETER + 16)) {
            return false;
        }
    }
    for (unsigned int i = 0U; i < POOL_CUSHION_COUNT; ++i) {
        for (unsigned int j = 0U; j < 2U; ++j) {
            const pool_vec_t center = point(j == 0U ? pool_cushions[i].a : pool_cushions[i].b);
            const int32_t radius    = (POOL_BALL_RADIUS + POOL_JAW_RADIUS) * POOL_ONE;
            if (speed_squared(difference(position, center)) < (uint64_t) radius * (uint64_t) radius) {
                return false;
            }
        }
    }
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        if (mouth_time(position, position, &pool_pockets[i]) >= 0) {
            return false;
        }
    }
    return true;
}

static void boundary(pool_ball_t* balls, unsigned int i, pool_vec_t start, pool_vec_t* remainder, uint16_t* potted,
                     pool_pot_events_t* events, pool_contact_state_t* contacts)
{
    if (potted == NULL) {
        cushions(&balls[i], remainder);
        return;
    }
    const int sink = open_boundary(&balls[i], start, remainder, contacts);
    if (sink >= 0 && (*potted & (1U << i)) == 0U) {
        *potted                         |= (uint16_t) (1U << i);
        events->ball[events->count]      = (uint8_t) i;
        events->pocket[events->count++]  = (uint8_t) sink;
    }
}

static bool impulse(pool_ball_t* a, pool_ball_t* b, pool_vec_t normal)
{
    int64_t distance2 = (int64_t) speed_squared(normal);
    if (distance2 == 0) {
        normal    = (pool_vec_t) {DIAMETER, 0}; /* Stable coincident-center fallback. */
        distance2 = (int64_t) DIAMETER * DIAMETER;
    }
    const pool_vec_t relative = difference(b->velocity, a->velocity);
    const int64_t approach    = (int64_t) relative.x * normal.x + (int64_t) relative.y * normal.y;
    if (approach >= 0) {
        return false;
    }
    /* Equal masses, restitution .97: impulse = -(1+.97)/2 * relative normal.
     * Using the unnormalized normal avoids a second normalization error. */
    const pool_vec_t change = {(int32_t) (-approach * normal.x * 197 / (distance2 * 200)),
                               (int32_t) (-approach * normal.y * 197 / (distance2 * 200))};
    const pool_vec_t av     = {a->velocity.x - change.x, a->velocity.y - change.y};
    const pool_vec_t bv     = {b->velocity.x + change.x, b->velocity.y + change.y};
    /* At nearly tangent, sub-Q12 impulses can round across the energy bound.
     * Reject that tiny impulse rather than add energy. Momentum stays exact. */
    if (speed_squared(av) + speed_squared(bv) > speed_squared(a->velocity) + speed_squared(b->velocity)) {
        return false;
    }
    a->velocity = av;
    b->velocity = bv;
    return change.x != 0 || change.y != 0;
}

static bool separate(pool_ball_t* a, pool_ball_t* b)
{
    pool_vec_t normal        = difference(b->position, a->position);
    const uint64_t distance2 = speed_squared(normal);
    if (distance2 >= (uint64_t) DIAMETER * DIAMETER) {
        return false;
    }
    uint32_t distance = square_root(distance2);
    if (distance == 0U) {
        normal   = (pool_vec_t) {POOL_ONE, 0};
        distance = POOL_ONE;
    }
    const int32_t penetration  = DIAMETER - (int32_t) square_root(distance2) + 8;
    const int32_t x            = (int32_t) ((int64_t) normal.x * penetration / (2 * distance));
    const int32_t y            = (int32_t) ((int64_t) normal.y * penetration / (2 * distance));
    a->position.x             -= x;
    a->position.y             -= y;
    b->position.x             += x;
    b->position.y             += y;
    return true;
}

static bool world_step(pool_ball_t* balls, unsigned int count, uint16_t* potted, pool_pot_events_t* events,
                       pool_contact_state_t* contacts)
{
    if (count == 0U || count > POOL_BALL_COUNT) {
        return false;
    }
    pool_vec_t remainder[POOL_BALL_COUNT] = {0};
    for (unsigned int i = 0U; i < count; ++i) {
        if (speed_squared(balls[i].velocity) < (uint64_t) (POOL_STOP_SPEED + 1) * (POOL_STOP_SPEED + 1)) {
            balls[i].velocity = (pool_vec_t) {0, 0};
        }
    }
    for (unsigned int step = 0U; step < POOL_SUBSTEPS; ++step) {
        pool_vec_t previous[POOL_BALL_COUNT];
        for (unsigned int i = 0U; i < count; ++i) {
            previous[i] = balls[i].position;
            if (potted != NULL) {
                if ((*potted & (1U << i)) != 0U) {
                    continue;
                }
                balls[i].position.x += (balls[i].velocity.x + remainder[i].x) / POOL_SUBSTEPS;
                balls[i].position.y += (balls[i].velocity.y + remainder[i].y) / POOL_SUBSTEPS;
                remainder[i].x       = (balls[i].velocity.x + remainder[i].x) % POOL_SUBSTEPS;
                remainder[i].y       = (balls[i].velocity.y + remainder[i].y) % POOL_SUBSTEPS;
                boundary(balls, i, previous[i], &remainder[i], potted, events, contacts);
                continue;
            }
            advance_axis(&balls[i].position.x, &balls[i].velocity.x, &remainder[i].x,
                         (POOL_TABLE_WIDTH - POOL_BALL_RADIUS) * POOL_ONE);
            advance_axis(&balls[i].position.y, &balls[i].velocity.y, &remainder[i].y,
                         (POOL_TABLE_HEIGHT - POOL_BALL_RADIUS) * POOL_ONE);
        }
        for (unsigned int pass = 0U; pass < CONTACT_PASSES; ++pass) {
            bool changed = false;
            for (unsigned int i = 0U; i < count; ++i) {
                for (unsigned int j = i + 1U; j < count; ++j) {
                    if (potted != NULL && (*potted & ((1U << i) | (1U << j))) != 0U) {
                        continue;
                    }
                    pool_ball_t* a         = &balls[i];
                    pool_ball_t* b         = &balls[j];
                    pool_vec_t normal      = difference(b->position, a->position);
                    const pool_vec_t start = difference(previous[j], previous[i]);
                    if ((normal.x > DIAMETER && start.x > DIAMETER) || (normal.x < -DIAMETER && start.x < -DIAMETER) ||
                        (normal.y > DIAMETER && start.y > DIAMETER) || (normal.y < -DIAMETER && start.y < -DIAMETER)) {
                        continue;
                    }
                    int32_t time = -1;
                    if (pass == 0U) {
                        time = circle_time(difference(previous[j], previous[i]), normal, DIAMETER);
                    }
                    if (time >= 0) {
                        const pool_vec_t at = interpolate(previous[i], a->position, time);
                        const pool_vec_t bt = interpolate(previous[j], b->position, time);
                        normal              = difference(bt, at);
                        if (impulse(a, b, normal)) {
                            if (contacts != NULL && contacts->first == 0U && i == 0U) {
                                contacts->first = j;
                            }
                            const int32_t remaining = TIME_ONE - time;
                            a->position             = (pool_vec_t) {
                                at.x + (int32_t) ((int64_t) a->velocity.x * remaining / (TIME_ONE * POOL_SUBSTEPS)),
                                at.y + (int32_t) ((int64_t) a->velocity.y * remaining / (TIME_ONE * POOL_SUBSTEPS))};
                            b->position = (pool_vec_t) {
                                bt.x + (int32_t) ((int64_t) b->velocity.x * remaining / (TIME_ONE * POOL_SUBSTEPS)),
                                bt.y + (int32_t) ((int64_t) b->velocity.y * remaining / (TIME_ONE * POOL_SUBSTEPS))};
                            remainder[i] = remainder[j] = (pool_vec_t) {0, 0};
                            changed                     = true;
                        }
                    } else if (speed_squared(normal) <= (uint64_t) DIAMETER * DIAMETER) {
                        if (impulse(a, b, normal)) {
                            if (contacts != NULL && contacts->first == 0U && i == 0U) {
                                contacts->first = j;
                            }
                            remainder[i] = remainder[j] = (pool_vec_t) {0, 0};
                            changed                     = true;
                        }
                    }
                    changed = separate(a, b) || changed;
                    boundary(balls, i, a->position, &remainder[i], potted, events, contacts);
                    boundary(balls, j, b->position, &remainder[j], potted, events, contacts);
                }
            }
            if (!changed) {
                break;
            }
        }
    }
    bool moving = false;
    for (unsigned int i = 0U; i < count; ++i) {
        if (potted != NULL && (*potted & (1U << i)) != 0U) {
            balls[i].velocity = (pool_vec_t) {0, 0};
            continue;
        }
        rolling_loss(&balls[i]);
        moving = speed_squared(balls[i].velocity) != 0U || moving;
    }
    return moving;
}

bool pool_physics_world_step(pool_ball_t* balls, unsigned int count)
{
    return world_step(balls, count, NULL, NULL, NULL);
}

bool pool_physics_play_step(pool_ball_t* balls, unsigned int count, uint16_t* potted, pool_pot_events_t* events)
{
    events->count = 0U;
    return world_step(balls, count, potted, events, NULL);
}

bool pool_physics_match_step(pool_ball_t* balls, unsigned int count, uint16_t* potted, pool_pot_events_t* events,
                             pool_contact_state_t* contacts)
{
    events->count = 0U;
    return world_step(balls, count, potted, events, contacts);
}

void pool_physics_step(pool_ball_t* ball)
{
    (void) pool_physics_world_step(ball, 1U);
}
