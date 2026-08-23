#ifndef STARFALL_GAME_H
#define STARFALL_GAME_H

#include <stdbool.h>
#include <stdint.h>

enum { STARFALL_WIDTH = 640, STARFALL_HEIGHT = 360 };
enum { STARFALL_STAR_COUNT = 96, STARFALL_SHOT_COUNT = 32,
       STARFALL_ENEMY_COUNT = 24, STARFALL_PARTICLE_COUNT = 64 };

typedef enum {
    STARFALL_TITLE,
    STARFALL_PLAYING,
    STARFALL_PAUSED,
    STARFALL_GAME_OVER,
} starfall_mode_t;

typedef struct {
    bool left, right, fire;
} starfall_input_t;

typedef struct {
    int16_t x, y;
    uint8_t speed;
} starfall_star_t;

typedef struct {
    int16_t x, y;
    bool active;
} starfall_shot_t;

typedef struct {
    int16_t x, y;
    int8_t dx;
    uint8_t kind;
    bool active;
} starfall_enemy_t;

typedef struct {
    int16_t x, y;
    int8_t dx, dy;
    uint8_t life;
    bool active;
} starfall_particle_t;

typedef struct {
    starfall_mode_t mode;
    starfall_input_t input;
    starfall_star_t stars[STARFALL_STAR_COUNT];
    starfall_shot_t shots[STARFALL_SHOT_COUNT];
    starfall_enemy_t enemies[STARFALL_ENEMY_COUNT];
    starfall_particle_t particles[STARFALL_PARTICLE_COUNT];
    int16_t player_x, player_y;
    uint32_t score, high_score, tick, wave;
    uint32_t next_fire_tick, next_spawn_tick, invulnerable_until;
    uint32_t damage_flash_until;
    uint8_t lives;
    uint32_t random_state;
} starfall_game_t;

void starfall_game_init(starfall_game_t *game, uint32_t seed, uint32_t high_score);
void starfall_game_start(starfall_game_t *game);
void starfall_game_toggle_pause(starfall_game_t *game);
void starfall_game_update(starfall_game_t *game);

#endif
