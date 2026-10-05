#ifndef POOL_INPUT_H
#define POOL_INPUT_H

#include <pool/game.h>
#include <tabos/input.h>

typedef struct {
        unsigned int held;
        unsigned int blocked; /* Transition-held keys must be released before reuse. */
        bool quit;
} pool_input_t;

bool pool_input_event(pool_input_t* input, pool_game_t* game, const tabos_input_event_t* event);
bool pool_input_active(const pool_input_t* input, const pool_game_t* game);
bool pool_input_step(const pool_input_t* input, pool_game_t* game);

#endif
