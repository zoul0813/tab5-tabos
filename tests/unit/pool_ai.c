#include <pool/ai.h>
#include <pool/input.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void choose(pool_ai_t* ai, pool_game_t* game)
{
    const uint16_t before = game->potted;
    pool_ball_t balls[POOL_BALL_COUNT];
    memcpy(balls, game->balls, sizeof(balls));
    for (unsigned int i = 0U; i < 90U; ++i) {
        assert(!pool_ai_step(ai, game));
        assert(ai->cursor == i + 1U);
        assert(game->potted == before && memcmp(balls, game->balls, sizeof(balls)) == 0);
    }
}
static void fire(pool_ai_t* ai, pool_game_t* game)
{
    unsigned int ticks = 0U;
    while (!game->shot_set && ticks++ < 110U) {
        (void) pool_ai_step(ai, game);
    }
    assert(game->shot_set);
    assert(game->angle < 4096U && game->power >= 1U && game->power <= 100U);
}
static void settle(pool_game_t* game)
{
    unsigned int ticks = 0U;
    while (game->shot_set && ticks++ < 600U) {
        (void) pool_game_step(game);
    }
    assert(!game->shot_set);
}
static pool_game_t single(void)
{
    pool_game_t game;
    pool_game_reset(&game);
    game.rules.break_shot  = false;
    game.potted            = (uint16_t) (UINT16_C(0xfffe) & ~(1U << 1U) & ~(1U << 8U));
    game.balls[0].position = (pool_vec_t) {264 * POOL_ONE, 180 * POOL_ONE};
    game.balls[1].position = (pool_vec_t) {264 * POOL_ONE, 80 * POOL_ONE};
    game.balls[8].position = (pool_vec_t) {450 * POOL_ONE, 200 * POOL_ONE};
    return game;
}
static void geometry(void)
{
    pool_game_t game = single();
    pool_ai_t ai;
    pool_ai_reset(&ai, 7U);
    choose(&ai, &game);
    assert(ai.found && ai.target == 1U && ai.pocket == 1U);
    assert(ai.angle == 3072U);
    fire(&ai, &game);
    settle(&game);
    assert((game.potted & (1U << 1U)) != 0U && game.rules.foul == POOL_FAIR);

    game                 = single();
    game.rules.groups[0] = POOL_STRIPES;
    game.rules.groups[1] = POOL_SOLIDS;
    pool_ai_reset(&ai, 7U);
    choose(&ai, &game);
    assert(!ai.found || ai.target == 8U);
    fire(&ai, &game);
    assert(game.rules.pending);

    /* An illegal ball blocks the easy cue path; do not choose that direct pot. */
    game                    = single();
    game.potted            &= (uint16_t) ~(1U << 9U);
    game.balls[9].position  = (pool_vec_t) {264 * POOL_ONE, 140 * POOL_ONE};
    game.rules.groups[0]    = POOL_SOLIDS;
    game.rules.groups[1]    = POOL_STRIPES;
    pool_ai_reset(&ai, 7U);
    choose(&ai, &game);
    assert(!ai.found || ai.target != 1U || ai.pocket != 1U);

    /* Obstruct the target-to-pocket corridor instead. */
    game.balls[9].position.y = 40 * POOL_ONE;
    pool_ai_reset(&ai, 7U);
    choose(&ai, &game);
    assert(!ai.found || ai.pocket != 1U);

    game                     = single();
    game.placement           = true;
    game.potted             |= 1U;
    game.placement_position  = game.balls[1].position;
    pool_ai_reset(&ai, 7U);
    choose(&ai, &game);
    assert(ai.found);
    const pool_vec_t object = game.balls[1].position;
    fire(&ai, &game);
    assert(!game.placement && (game.potted & 1U) == 0U);
    assert(game.balls[1].position.x == object.x && game.balls[1].position.y == object.y);
    settle(&game);
    assert((game.potted & (1U << 1U)) != 0U);

    /* A shallow line to the top side pocket meets its jaw/face. */
    game                   = single();
    game.balls[1].position = (pool_vec_t) {120 * POOL_ONE, 12 * POOL_ONE};
    game.balls[0].position = (pool_vec_t) {60 * POOL_ONE, 20 * POOL_ONE};
    pool_ai_reset(&ai, 7U);
    choose(&ai, &game);
    assert(!ai.found || ai.pocket != 1U);

    /* A dense rack with ball in hand must use a valid finite fallback. */
    pool_game_reset(&game);
    game.placement          = true;
    game.potted             = 1U;
    game.placement_position = game.balls[1].position;
    pool_ai_reset(&ai, 9U);
    choose(&ai, &game);
    fire(&ai, &game);
    assert(!game.placement && (game.potted & 1U) == 0U);
    assert(pool_physics_placement_valid(game.balls, game.potted, game.balls[0].position));
    settle(&game);

    /* A rack supplies no direct pot: finite fallback still shoots. */
    pool_game_reset(&game);
    pool_ai_reset(&ai, 3U);
    choose(&ai, &game);
    assert(!ai.found);
    fire(&ai, &game);
    settle(&game);
    assert(game.contacts.first == 1U);
}
static void controls(void)
{
    pool_game_t game            = single();
    game.computer               = true;
    game.rules.player           = 1U;
    pool_input_t input          = {0};
    const tabos_key_t blocked[] = {TABOS_KEY_LEFT, TABOS_KEY_W, TABOS_KEY_SPACE, TABOS_KEY_ENTER};
    for (unsigned int i = 0U; i < 4U; ++i) {
        tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = blocked[i]};
        assert(!pool_input_event(&input, &game, &event));
        assert(!pool_input_step(&input, &game));
        assert(!pool_input_active(&input, &game));
    }
    assert(game.angle == 0U && game.power == 50U && !game.shot_set);
    pool_ai_t ai;
    pool_ai_reset(&ai, 77U);
    (void) pool_ai_step(&ai, &game);
    game.paused = true;
    assert(!pool_ai_step(&ai, &game) && ai.cursor == 1U);
    game.paused          = false;
    game.restart_pending = true;
    assert(!pool_ai_step(&ai, &game) && ai.cursor == 1U);
    game.restart_pending = false;
    (void) pool_ai_step(&ai, &game);
    assert(ai.cursor == 2U);
    game.rules.complete = true;
    assert(!pool_ai_step(&ai, &game));
    pool_game_new_rack(&game);
    assert(game.computer && game.rules.player == 0U && !game.rules.complete);
}
static uint64_t mix(uint64_t hash, uint32_t value)
{
    return (hash ^ value) * UINT64_C(1099511628211);
}
static uint64_t rack(uint32_t seed, unsigned int* shots)
{
    pool_game_t game;
    pool_game_reset(&game);
    pool_ai_t ai;
    pool_ai_reset(&ai, seed);
    uint64_t hash = UINT64_C(1469598103934665603);
    for (*shots = 0U; *shots < 400U && !game.rules.complete; ++*shots) {
        fire(&ai, &game);
        hash = mix(mix(hash, game.angle), game.power);
        settle(&game);
        for (unsigned int i = 0U; i < 16U; ++i) {
            hash = mix(mix(hash, (uint32_t) game.balls[i].position.x), (uint32_t) game.balls[i].position.y);
        }
        hash = mix(mix(hash, game.potted), game.rules.player);
    }
    printf("AI seed %u: %u shots, complete %u, mask %04x, hash %llu\n", seed, *shots, game.rules.complete ? 1U : 0U,
           game.potted, (unsigned long long) hash);
    assert(game.rules.complete);
    static const uint64_t expected[] = {UINT64_C(1553807515911589606), UINT64_C(16654460646833064518),
                                        UINT64_C(14459245599298524135), UINT64_C(730697337411794334)};
    assert(hash == expected[seed - 1U]);
    assert(!game.rules.early_eight && game.rules.foul == POOL_FAIR);
    return mix(hash, game.rules.winner);
}
int main(void)
{
    geometry();
    controls();
    for (uint32_t seed = 1U; seed <= 4U; ++seed) {
        unsigned int shots_a, shots_b;
        const uint64_t first = rack(seed, &shots_a);
        assert(first == rack(seed, &shots_b) && shots_a == shots_b);
    }
    puts("Pool bounded AI geometry, input, placement and deterministic full racks passed");
    return 0;
}
