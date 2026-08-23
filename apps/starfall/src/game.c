#include <starfall/game.h>

#include <string.h>

enum {
    PLAYER_WIDTH  = 20,
    PLAYER_HEIGHT = 16,
    PLAYER_SPEED  = 4,
    ENEMY_WIDTH   = 18,
    ENEMY_HEIGHT  = 14,
    SHOT_WIDTH    = 3,
    SHOT_HEIGHT   = 8
};

static uint32_t random_next(starfall_game_t* game)
{
    uint32_t value      = game->random_state != 0U ? game->random_state : 0x51a7f411U;
    value              ^= value << 13U;
    value              ^= value >> 17U;
    value              ^= value << 5U;
    game->random_state  = value;
    return value;
}

static bool overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void initialize_stars(starfall_game_t* game)
{
    for (unsigned int index = 0U; index < STARFALL_STAR_COUNT; ++index) {
        game->stars[index].x     = (int16_t) (random_next(game) % STARFALL_WIDTH);
        game->stars[index].y     = (int16_t) (random_next(game) % STARFALL_HEIGHT);
        game->stars[index].speed = (uint8_t) (1U + random_next(game) % 3U);
    }
}

void starfall_game_init(starfall_game_t* game, uint32_t seed, uint32_t high_score)
{
    memset(game, 0, sizeof(*game));
    game->random_state = seed;
    game->high_score   = high_score;
    game->mode         = STARFALL_TITLE;
    initialize_stars(game);
}

void starfall_game_start(starfall_game_t* game)
{
    const uint32_t seed       = game->random_state;
    const uint32_t high_score = game->high_score;
    starfall_game_init(game, seed, high_score);
    game->mode            = STARFALL_PLAYING;
    game->player_x        = (STARFALL_WIDTH - PLAYER_WIDTH) / 2;
    game->player_y        = STARFALL_HEIGHT - 42;
    game->lives           = 3U;
    game->wave            = 1U;
    game->next_spawn_tick = 90U;
}

void starfall_game_toggle_pause(starfall_game_t* game)
{
    if (game->mode == STARFALL_PLAYING) {
        game->mode = STARFALL_PAUSED;
    } else if (game->mode == STARFALL_PAUSED) {
        game->mode = STARFALL_PLAYING;
    }
}

static void emit_particles(starfall_game_t* game, int x, int y)
{
    for (unsigned int emitted = 0U; emitted < 12U; ++emitted) {
        for (unsigned int index = 0U; index < STARFALL_PARTICLE_COUNT; ++index) {
            starfall_particle_t* particle = &game->particles[index];
            if (particle->active) {
                continue;
            }
            particle->active = true;
            particle->x      = (int16_t) x;
            particle->y      = (int16_t) y;
            particle->dx     = (int8_t) ((int) (random_next(game) % 5U) - 2);
            particle->dy     = (int8_t) ((int) (random_next(game) % 5U) - 2);
            particle->life   = (uint8_t) (12U + random_next(game) % 14U);
            break;
        }
    }
}

static void spawn_enemy(starfall_game_t* game)
{
    for (unsigned int index = 0U; index < STARFALL_ENEMY_COUNT; ++index) {
        starfall_enemy_t* enemy = &game->enemies[index];
        if (enemy->active) {
            continue;
        }
        enemy->active = true;
        enemy->kind   = (uint8_t) (random_next(game) % 3U);
        enemy->x      = (int16_t) (12U + random_next(game) % (STARFALL_WIDTH - 42U));
        enemy->y      = 28;
        enemy->dx     = (random_next(game) & 1U) != 0U ? 1 : -1;
        return;
    }
}

static void fire(starfall_game_t* game)
{
    if (!game->input.fire || game->tick < game->next_fire_tick) {
        return;
    }
    for (unsigned int index = 0U; index < STARFALL_SHOT_COUNT; ++index) {
        if (game->shots[index].active) {
            continue;
        }
        game->shots[index] = (starfall_shot_t) {
            .x      = (int16_t) (game->player_x + PLAYER_WIDTH / 2 - 1),
            .y      = (int16_t) (game->player_y - SHOT_HEIGHT),
            .active = true,
        };
        game->next_fire_tick = game->tick + 8U;
        return;
    }
}

static void damage_player(starfall_game_t* game)
{
    if (game->tick < game->invulnerable_until) {
        return;
    }
    if (game->lives > 0U) {
        --game->lives;
    }
    game->damage_flash_until = game->tick + 10U;
    game->invulnerable_until = game->tick + 90U;
    emit_particles(game, game->player_x + PLAYER_WIDTH / 2, game->player_y + PLAYER_HEIGHT / 2);
    if (game->lives == 0U) {
        game->mode = STARFALL_GAME_OVER;
        if (game->score > game->high_score) {
            game->high_score = game->score;
        }
    }
}

void starfall_game_update(starfall_game_t* game)
{
    for (unsigned int index = 0U; index < STARFALL_STAR_COUNT; ++index) {
        starfall_star_t* star = &game->stars[index];
        star->y               = (int16_t) (star->y + star->speed);
        if (star->y >= STARFALL_HEIGHT) {
            star->y = 24;
            star->x = (int16_t) (random_next(game) % STARFALL_WIDTH);
        }
    }
    if (game->mode != STARFALL_PLAYING) {
        return;
    }
    ++game->tick;

    game->player_x += (game->input.right ? PLAYER_SPEED : 0) - (game->input.left ? PLAYER_SPEED : 0);
    if (game->player_x < 0) {
        game->player_x = 0;
    }
    if (game->player_x > STARFALL_WIDTH - PLAYER_WIDTH) {
        game->player_x = STARFALL_WIDTH - PLAYER_WIDTH;
    }
    fire(game);

    if (game->tick >= game->next_spawn_tick) {
        spawn_enemy(game);
        const uint32_t interval = game->wave < 11U ? 95U - game->wave * 5U : 40U;
        game->next_spawn_tick   = game->tick + interval;
    }
    game->wave = 1U + game->score / 1000U;

    for (unsigned int index = 0U; index < STARFALL_SHOT_COUNT; ++index) {
        starfall_shot_t* shot = &game->shots[index];
        if (!shot->active) {
            continue;
        }
        shot->y = (int16_t) (shot->y - 7);
        if (shot->y < 22) {
            shot->active = false;
        }
    }
    for (unsigned int index = 0U; index < STARFALL_ENEMY_COUNT; ++index) {
        starfall_enemy_t* enemy = &game->enemies[index];
        if (!enemy->active) {
            continue;
        }
        enemy->y = (int16_t) (enemy->y + 1 + (int) (game->wave / 6U));
        if (enemy->kind == 1U) {
            enemy->x = (int16_t) (enemy->x + enemy->dx * 2);
            if (enemy->x < 2 || enemy->x > STARFALL_WIDTH - ENEMY_WIDTH - 2) {
                enemy->dx = (int8_t) -enemy->dx;
            }
        } else if (enemy->kind == 2U && enemy->y > 100) {
            enemy->x = (int16_t) (enemy->x + (game->player_x > enemy->x ? 1 : -1));
        }
        if (enemy->y >= STARFALL_HEIGHT) {
            enemy->active = false;
            continue;
        }
        for (unsigned int shot_index = 0U; shot_index < STARFALL_SHOT_COUNT; ++shot_index) {
            starfall_shot_t* shot = &game->shots[shot_index];
            if (!shot->active ||
                !overlaps(shot->x, shot->y, SHOT_WIDTH, SHOT_HEIGHT, enemy->x, enemy->y, ENEMY_WIDTH, ENEMY_HEIGHT)) {
                continue;
            }
            shot->active   = false;
            enemy->active  = false;
            game->score   += 100U + enemy->kind * 50U;
            if (game->score > game->high_score) {
                game->high_score = game->score;
            }
            emit_particles(game, enemy->x + ENEMY_WIDTH / 2, enemy->y + ENEMY_HEIGHT / 2);
            break;
        }
        if (enemy->active && overlaps(game->player_x, game->player_y, PLAYER_WIDTH, PLAYER_HEIGHT, enemy->x, enemy->y,
                                      ENEMY_WIDTH, ENEMY_HEIGHT)) {
            enemy->active = false;
            damage_player(game);
        }
    }
    for (unsigned int index = 0U; index < STARFALL_PARTICLE_COUNT; ++index) {
        starfall_particle_t* particle = &game->particles[index];
        if (!particle->active) {
            continue;
        }
        particle->x = (int16_t) (particle->x + particle->dx);
        particle->y = (int16_t) (particle->y + particle->dy);
        if (--particle->life == 0U) {
            particle->active = false;
        }
    }
}
