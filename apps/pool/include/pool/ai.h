#ifndef POOL_AI_H
#define POOL_AI_H
#include <pool/game.h>

/* One target/pocket candidate per tick; no simulated lookahead or allocation. */
typedef struct {
        unsigned int cursor;
        unsigned int delay;
        unsigned int target;
        unsigned int pocket;
        unsigned int angle;
        unsigned int power;
        int score;
        pool_vec_t cue;
        uint32_t seed;
        bool found;
        bool prepared;
} pool_ai_t;

void pool_ai_reset(pool_ai_t* ai, uint32_t seed);
/* May also be used for either player in deterministic self-play tests. */
bool pool_ai_step(pool_ai_t* ai, pool_game_t* game);
#endif
