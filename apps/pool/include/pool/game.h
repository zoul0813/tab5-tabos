#ifndef POOL_GAME_H
#define POOL_GAME_H

#include <pool/rules.h>
#include <stdbool.h>
#include <stdint.h>

enum {
    POOL_ANGLE_COUNT   = 4096,
    POOL_DEFAULT_POWER = 50,
    POOL_AIM_STEP      = 11,
    POOL_FINE_STEP     = 1
};

typedef struct {
        pool_ball_t balls[POOL_BALL_COUNT];
        unsigned int angle; /* Clockwise from right, modulo 4096. */
        unsigned int power; /* 1..100, independent of input hold time. */
        uint16_t potted;    /* Bit 0 removes the cue for scratch/ball-in-hand placement. */
        uint8_t pocket_for_ball[POOL_BALL_COUNT];
        pool_pot_events_t shot_pots;
        pool_rules_t rules;
        pool_contact_state_t contacts;
        bool title;
        bool help;
        bool muted;
        unsigned int cue_flash;
        unsigned int pot_flash[POOL_POCKET_COUNT];
        pool_vec_t cue_origin;
        unsigned int cue_angle;
        bool computer; /* Player 2 is controlled by the computer. */
        bool restart_pending;
        bool placement;
        pool_vec_t placement_position;
        bool paused;
        bool shot_set; /* A shot is in progress; cleared on exact rest. */
} pool_game_t;

void pool_game_reset(pool_game_t* game);
void pool_game_new_rack(pool_game_t* game);
bool pool_game_computer_turn(const pool_game_t* game);
bool pool_game_can_aim(const pool_game_t* game);
bool pool_game_can_place(const pool_game_t* game);
bool pool_game_placement_valid(const pool_game_t* game);
bool pool_game_confirm_placement(pool_game_t* game);
bool pool_game_adjust(pool_game_t* game, int aim, int power, bool fine);
bool pool_game_shoot(pool_game_t* game);
bool pool_game_step(pool_game_t* game);
bool pool_game_animating(const pool_game_t* game);
bool pool_game_effects_step(pool_game_t* game);
pool_vec_t pool_direction(unsigned int angle);
/* Short guide clipped to the closed practice-table center bounds. */
pool_vec_t pool_game_aim_end(const pool_game_t* game);

#endif
