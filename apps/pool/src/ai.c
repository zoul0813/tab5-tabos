#include <pool/ai.h>
#include <limits.h>
#include <string.h>

/* Geometry uses 1/16 pixels. Table-bounded coordinates keep squared cross
 * products below 2^56. The authoritative simulation remains Q12. */
enum {
    SCALE    = 16,
    DIAMETER = 14 * SCALE,
    SEARCH   = 15 * 6
};
static pool_vec_t point(pool_vec_t v)
{
    return (pool_vec_t) {v.x / (POOL_ONE / SCALE), v.y / (POOL_ONE / SCALE)};
}
static pool_vec_t delta(pool_vec_t a, pool_vec_t b)
{
    return (pool_vec_t) {a.x - b.x, a.y - b.y};
}
static int64_t dot(pool_vec_t a, pool_vec_t b)
{
    return (int64_t) a.x * b.x + (int64_t) a.y * b.y;
}
static uint32_t root(uint64_t value)
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
static int32_t length(pool_vec_t v)
{
    return (int32_t) root((uint64_t) dot(v, v));
}
static bool near_segment(pool_vec_t a, pool_vec_t b, pool_vec_t c, int radius)
{
    const pool_vec_t line = delta(b, a), offset = delta(c, a);
    const int64_t square = dot(line, line), along = dot(offset, line);
    if (along <= 0 || square == 0) {
        return dot(offset, offset) < (int64_t) radius * radius;
    }
    if (along >= square) {
        const pool_vec_t end = delta(c, b);
        return dot(end, end) < (int64_t) radius * radius;
    }
    const int64_t cross = (int64_t) line.x * offset.y - (int64_t) line.y * offset.x;
    return cross * cross < (int64_t) radius * radius * square;
}
static bool clear(const pool_game_t* game, pool_vec_t a, pool_vec_t b, unsigned int target, bool rails)
{
    for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
        if (i != target && (game->potted & (1U << i)) == 0U &&
            near_segment(a, b, point(game->balls[i].position), DIAMETER + 2)) {
            return false;
        }
    }
    if (!rails) {
        return true;
    }
    for (unsigned int i = 0U; i < POOL_CUSHION_COUNT; ++i) {
        const pool_cushion_t* c = &pool_cushions[i];
        const pool_vec_t ends[] = {
            {c->a.x * SCALE, c->a.y * SCALE},
            {c->b.x * SCALE, c->b.y * SCALE}
        };
        for (unsigned int j = 0U; j < 2U; ++j) {
            if (near_segment(a, b, ends[j], 9 * SCALE + 2)) {
                return false;
            }
        }
        const bool horizontal = c->a.y == c->b.y;
        const int edge        = horizontal ? c->a.y : c->a.x;
        const int face        = (edge == 0 ? 7 : edge - 7) * SCALE;
        const int start       = horizontal ? a.y : a.x;
        const int finish      = horizontal ? b.y : b.x;
        if (start == finish || (start < face) == (finish < face)) {
            continue;
        }
        const int other_start  = horizontal ? a.x : a.y;
        const int other_finish = horizontal ? b.x : b.y;
        const int at = other_start + (int) ((int64_t) (other_finish - other_start) * (face - start) / (finish - start));
        const int low  = (horizontal ? c->a.x : c->a.y) * SCALE;
        const int high = (horizontal ? c->b.x : c->b.y) * SCALE;
        if (at >= low && at <= high) {
            return false;
        }
    }
    return true;
}
static unsigned int angle_of(pool_vec_t v)
{
    const int32_t x  = v.x < 0 ? -v.x : v.x;
    const int32_t y  = v.y < 0 ? -v.y : v.y;
    unsigned int low = 0U, high = 1024U;
    while (low < high) {
        const unsigned int mid = (low + high) / 2U;
        const pool_vec_t d     = pool_direction(mid);
        if ((int64_t) d.y * x < (int64_t) d.x * y) {
            low = mid + 1U;
        } else {
            high = mid;
        }
    }
    unsigned int result = v.x < 0 ? 2048U - low : low;
    if (v.y < 0) {
        result = (4096U - result) % 4096U;
    }
    return result;
}
static uint32_t random_next(pool_ai_t* ai)
{
    ai->seed = ai->seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return ai->seed;
}
void pool_ai_reset(pool_ai_t* ai, uint32_t seed)
{
    memset(ai, 0, sizeof(*ai));
    ai->seed  = seed;
    ai->score = INT_MAX;
}
static void candidate(pool_ai_t* ai, const pool_game_t* game, unsigned int target, unsigned int pocket)
{
    if (!pool_rules_legal_target(&game->rules, game->potted, target)) {
        return;
    }
    const pool_vec_t ball = point(game->balls[target].position);
    const pool_vec_t sink = {pool_pockets[pocket].center.x * SCALE, pool_pockets[pocket].center.y * SCALE};
    const pool_vec_t path = delta(sink, ball);
    const int32_t travel  = length(path);
    if (travel == 0) {
        return;
    }
    const pool_vec_t ghost = {ball.x - (int32_t) ((int64_t) path.x * DIAMETER / travel),
                              ball.y - (int32_t) ((int64_t) path.y * DIAMETER / travel)};
    pool_vec_t cue         = point(game->balls[0].position);
    if (game->placement) {
        cue = (pool_vec_t) {ghost.x - (int32_t) ((int64_t) path.x * 50 * SCALE / travel),
                            ghost.y - (int32_t) ((int64_t) path.y * 50 * SCALE / travel)};
    }
    const pool_vec_t qcue   = {cue.x * (POOL_ONE / SCALE), cue.y * (POOL_ONE / SCALE)};
    const pool_vec_t qghost = {ghost.x * (POOL_ONE / SCALE), ghost.y * (POOL_ONE / SCALE)};
    if (!pool_physics_placement_valid(game->balls, (uint16_t) (game->potted | (1U << target)), qghost) ||
        (game->placement && !pool_physics_placement_valid(game->balls, game->potted, qcue))) {
        return;
    }
    const pool_vec_t approach = delta(ghost, cue);
    const int32_t distance    = length(approach);
    if (distance == 0) {
        return;
    }
    const int64_t cosine = dot(approach, path) * 1024 / ((int64_t) distance * travel);
    if (cosine < 600 || !clear(game, cue, ghost, target, true) || !clear(game, ball, sink, target, true)) {
        return;
    }
    const int effective = distance / SCALE + (int) ((int64_t) travel * 1024 * 1024 / (cosine * cosine * SCALE));
    int score           = effective + (int) (1024 - cosine);
    /* Prefer keeping the contact point away from pocket mouths; this is a
     * conservative scratch-risk heuristic, not a prediction of cue travel. */
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        const pool_vec_t p = {pool_pockets[i].center.x * SCALE, pool_pockets[i].center.y * SCALE};
        if (length(delta(ghost, p)) < 35 * SCALE) {
            score += 150;
        }
    }
    if (score >= ai->score) {
        return;
    }
    const unsigned int speed = root((uint64_t) effective * 201U + 10000U);
    unsigned int power       = 1U;
    if (speed > 30U) {
        power = 1U + ((speed - 30U) * 99U + 569U) / 570U;
    }
    ai->score  = score;
    ai->found  = true;
    ai->target = target;
    ai->pocket = pocket;
    ai->cue    = qcue;
    ai->angle  = angle_of(approach);
    ai->power  = power > 100U ? 100U : power;
}
static void fallback(pool_ai_t* ai, pool_game_t* game)
{
    if (game->placement) {
        /* 18 x 9 nonoverlapping candidate spots, more than the 15 blockers. */
        for (unsigned int i = 0U; i < 162U; ++i) {
            const pool_vec_t spot = {(20 + (int32_t) (i % 18U) * 28) * POOL_ONE,
                                     (20 + (int32_t) (i / 18U) * 28) * POOL_ONE};
            if (pool_physics_placement_valid(game->balls, game->potted, spot)) {
                ai->cue = spot;
                break;
            }
        }
    } else {
        ai->cue = game->balls[0].position;
    }
    int best = INT_MAX;
    for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
        if (!pool_rules_legal_target(&game->rules, game->potted, i)) {
            continue;
        }
        const pool_vec_t cue = point(ai->cue), ball = point(game->balls[i].position);
        const pool_vec_t path = delta(ball, cue);
        const int score       = length(path) + (clear(game, cue, ball, i, true) ? 0 : 100000);
        if (score < best) {
            best       = score;
            ai->target = i;
            ai->angle  = angle_of(path);
            ai->power  = game->rules.break_shot ? 100U : 65U;
        }
    }
}
bool pool_ai_step(pool_ai_t* ai, pool_game_t* game)
{
    if (game->paused || game->restart_pending || game->rules.complete) {
        return false;
    }
    if (!pool_game_can_aim(game) && !pool_game_can_place(game)) {
        pool_ai_reset(ai, ai->seed);
        return false;
    }
    if (ai->cursor < SEARCH) {
        const unsigned int index = ai->cursor++;
        candidate(ai, game, index / 6U + 1U, index % 6U);
        return false;
    }
    if (!ai->prepared) {
        if (!ai->found) {
            fallback(ai, game);
        }
        if (game->placement) {
            game->placement_position = ai->cue;
            if (!pool_game_confirm_placement(game)) {
                return false;
            }
        }
        const int error = (int) (random_next(ai) % 7U) - 3;
        game->angle     = (unsigned int) ((int) ai->angle + 4096 + error) % 4096U;
        int power       = (int) ai->power + (int) (random_next(ai) % 5U) - 2;
        if (power < 1) {
            power = 1;
        }
        if (power > 100) {
            power = 100;
        }
        game->power  = (unsigned int) power;
        ai->prepared = true;
        return true;
    }
    if (++ai->delay < 18U) {
        return false;
    }
    const bool shot = pool_game_shoot(game);
    pool_ai_reset(ai, ai->seed);
    return shot;
}
