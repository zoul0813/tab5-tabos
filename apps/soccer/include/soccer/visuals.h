#ifndef SOCCER_VISUALS_H
#define SOCCER_VISUALS_H
#include <soccer/game.h>

enum {
    SOCCER_VISUAL_ACTORS = SOCCER_PLAYER_COUNT + 2
};

/* Presentation state. It observes simulation snapshots and never changes them. */
typedef struct {
        soccer_vec_t position[SOCCER_VISUAL_ACTORS], ball_position;
        uint32_t run_distance[SOCCER_VISUAL_ACTORS], ball_distance;
        unsigned int previous_kick[SOCCER_VISUAL_ACTORS];
        int action_x[SOCCER_VISUAL_ACTORS], action_y[SOCCER_VISUAL_ACTORS];
        soccer_event_t action[SOCCER_VISUAL_ACTORS];
        bool moving[SOCCER_VISUAL_ACTORS];
        soccer_mode_t mode;
} soccer_visuals_t;
void soccer_visuals_reset(soccer_visuals_t* visuals, const soccer_game_t* game);
void soccer_visuals_step(soccer_visuals_t* visuals, const soccer_game_t* game);
#endif
