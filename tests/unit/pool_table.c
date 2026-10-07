#include <pool/table.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static bool same(pool_point_t a, pool_point_t b)
{
    return a.x == b.x && a.y == b.y;
}

int main(void)
{
    assert(POOL_TABLE_WIDTH == 2 * POOL_TABLE_HEIGHT);
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        const pool_point_t a = pool_initial_balls[i];
        assert(a.x >= POOL_BALL_RADIUS && a.x <= POOL_TABLE_WIDTH - POOL_BALL_RADIUS);
        assert(a.y >= POOL_BALL_RADIUS && a.y <= POOL_TABLE_HEIGHT - POOL_BALL_RADIUS);
        for (unsigned int j = i + 1U; j < POOL_BALL_COUNT; ++j) {
            const pool_point_t b = pool_initial_balls[j];
            const int dx         = a.x - b.x;
            const int dy         = a.y - b.y;
            assert(dx * dx + dy * dy >= 4 * POOL_BALL_RADIUS * POOL_BALL_RADIUS);
        }
    }
    assert(pool_initial_balls[0].x == POOL_TABLE_WIDTH / 4);
    assert(pool_initial_balls[1].x == POOL_TABLE_WIDTH * 3 / 4);
    assert(pool_initial_balls[8].y == POOL_TABLE_HEIGHT / 2);
    assert(pool_initial_balls[8].x > pool_initial_balls[1].x);
    assert(pool_initial_balls[6].x == pool_initial_balls[15].x);
    /* Every mouth terminates at two distinct cushion jaws. Every jaw belongs
     * to exactly one opening; there is no hidden uninterrupted rectangle. */
    unsigned int uses[POOL_CUSHION_COUNT][2] = {{0U}};
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        const pool_pocket_t* pocket = &pool_pockets[i];
        assert(pocket->radius > POOL_BALL_RADIUS);
        const int dx        = pocket->mouth_a.x - pocket->mouth_b.x;
        const int dy        = pocket->mouth_a.y - pocket->mouth_b.y;
        const int clearance = 2 * (POOL_BALL_RADIUS + POOL_JAW_RADIUS);
        assert(dx * dx + dy * dy > clearance * clearance);
        const pool_point_t mouths[] = {pocket->mouth_a, pocket->mouth_b};
        for (unsigned int m = 0U; m < 2U; ++m) {
            unsigned int matches = 0U;
            for (unsigned int c = 0U; c < POOL_CUSHION_COUNT; ++c) {
                const pool_point_t ends[] = {pool_cushions[c].a, pool_cushions[c].b};
                for (unsigned int e = 0U; e < 2U; ++e) {
                    if (same(mouths[m], ends[e])) {
                        ++matches;
                        ++uses[c][e];
                    }
                }
            }
            assert(matches == 1U);
        }
        assert(pocket->center.y < 0 || pocket->center.y > POOL_TABLE_HEIGHT);
    }
    for (unsigned int c = 0U; c < POOL_CUSHION_COUNT; ++c) {
        assert(uses[c][0] == 1U && uses[c][1] == 1U);
        const pool_cushion_t* cushion = &pool_cushions[c];
        assert(!same(cushion->a, cushion->b));
        assert(cushion->a.x == cushion->b.x || cushion->a.y == cushion->b.y);
    }
    puts("Pool geometry: rack clearances and six connected pocket mouths passed");
    return 0;
}
