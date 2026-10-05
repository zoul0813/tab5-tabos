#ifndef POOL_PHYSICS_H
#define POOL_PHYSICS_H

#include <pool/table.h>
#include <stdint.h>
#include <stdbool.h>

enum {
    POOL_ONE        = 4096,
    POOL_SUBSTEPS   = 4,
    POOL_FRICTION   = 114, /* Q12 px/tick lost per tick: 100.195 px/s². */
    POOL_STOP_SPEED = 137  /* Q12 px/tick: 2.007 px/s. */
};

typedef struct {
        int32_t x, y;
} pool_vec_t;

typedef struct {
        pool_vec_t position;
        pool_vec_t velocity; /* Q12 logical pixels per 60 Hz tick. */
} pool_ball_t;

/* One 60 Hz tick, four substeps, closed rectangular practice cushions.
 * Input center must be within the table; velocity components <= 10 * ONE in magnitude.
 * No ball contacts, pocket capture, allocation, clocks or SDK dependency. */
void pool_physics_step(pool_ball_t* ball);
/* Stable ID order, 1..16 balls; returns whether any velocity remains. */
bool pool_physics_world_step(pool_ball_t* balls, unsigned int count);

typedef struct {
        unsigned int count;
        uint8_t ball[POOL_BALL_COUNT];
        uint8_t pocket[POOL_BALL_COUNT];
} pool_pot_events_t;

/* Open mouths and finite jaws. Potted bits are persistent; events reset per tick.
 * Captured balls have zero velocity and never enter further contacts. */
bool pool_physics_play_step(pool_ball_t* balls, unsigned int count, uint16_t* potted, pool_pot_events_t* events);
bool pool_physics_placement_valid(const pool_ball_t* balls, uint16_t potted, pool_vec_t position);

/* Observational shot trace; never changes impulses, positions or pot order. */
typedef struct {
        unsigned int first; /* First cue/object impulse, 0 until contact. */
        bool rail_after;    /* Any subsequent ball/rail or jaw reflection. */
} pool_contact_state_t;
bool pool_physics_match_step(pool_ball_t* balls, unsigned int count, uint16_t* potted, pool_pot_events_t* events,
                             pool_contact_state_t* contacts);

#endif
