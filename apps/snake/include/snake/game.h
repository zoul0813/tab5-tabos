#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <stdbool.h>
#include <stdint.h>

enum {
    SNAKE_COLUMNS  = 32,
    SNAKE_ROWS     = 18,
    SNAKE_CAPACITY = SNAKE_COLUMNS * SNAKE_ROWS
};
typedef enum {
    SNAKE_UP,
    SNAKE_RIGHT,
    SNAKE_DOWN,
    SNAKE_LEFT
} snake_direction_t;
typedef enum {
    SNAKE_TITLE,
    SNAKE_PLAYING,
    SNAKE_PAUSED,
    SNAKE_OVER,
    SNAKE_WON
} snake_mode_t;

typedef struct {
        uint16_t cells[SNAKE_CAPACITY]; /* Head first, tail last. */
        uint16_t length;
        uint16_t food;
        uint32_t random;
        uint32_t best;
        uint64_t deadline;
        snake_direction_t direction;
        snake_direction_t turns[2];
        unsigned int turn_count;
        snake_mode_t mode;
} snake_game_t;

void snake_game_init(snake_game_t* game, uint32_t seed);
void snake_game_start(snake_game_t* game, uint64_t now);
bool snake_game_turn(snake_game_t* game, snake_direction_t direction);
void snake_game_pause(snake_game_t* game, uint64_t now);
bool snake_game_update(snake_game_t* game, uint64_t now);
uint32_t snake_game_score(const snake_game_t* game);
uint32_t snake_game_interval(const snake_game_t* game);

#endif
