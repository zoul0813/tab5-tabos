#ifndef SOCCER_GAME_H
#define SOCCER_GAME_H
#include <stdbool.h>
#include <stdint.h>
enum {
    SOCCER_MATCH_TICKS                  = 180 * 60,
    SOCCER_TEAM_SIZE                    = 5,
    SOCCER_DEFENDER_FIRST               = 3,
    SOCCER_PLAYER_COUNT                 = 2 * SOCCER_TEAM_SIZE,
    SOCCER_ONE                          = 256,
    SOCCER_PITCH_CENTRE_Y               = 480,
    SOCCER_GOAL_HEIGHT                  = 40,
    SOCCER_BALL_RADIUS                  = 4,
    SOCCER_GOAL_POST_RADIUS             = 4,
    SOCCER_GOAL_COLLISION_RADIUS        = SOCCER_BALL_RADIUS + SOCCER_GOAL_POST_RADIUS,
    SOCCER_GOAL_SCORING_CLEARANCE       = SOCCER_GOAL_COLLISION_RADIUS,
    SOCCER_GOAL_LINE_CROSSING_OFFSET    = SOCCER_BALL_RADIUS,
    SOCCER_LEFT                         = 32,
    SOCCER_RIGHT                        = 1568,
    SOCCER_TOP                          = 32,
    SOCCER_BOTTOM                       = 928,
    SOCCER_GOAL_TOP                     = 408,
    SOCCER_GOAL_BOTTOM                  = 552,
    SOCCER_GOAL_CENTRE_Y                = (SOCCER_GOAL_TOP + SOCCER_GOAL_BOTTOM) / 2,
    SOCCER_GOAL_SCORE_TOP               = SOCCER_GOAL_TOP + SOCCER_GOAL_SCORING_CLEARANCE,
    SOCCER_GOAL_SCORE_BOTTOM            = SOCCER_GOAL_BOTTOM - SOCCER_GOAL_SCORING_CLEARANCE,
    SOCCER_GOAL_LANE_OFFSET             = (SOCCER_GOAL_SCORE_BOTTOM - SOCCER_GOAL_SCORE_TOP) * 3 / 8,
    SOCCER_KEEPER_STANDING_CLEARANCE    = 12,
    SOCCER_KEEPER_STANDING_TOP          = SOCCER_GOAL_TOP + SOCCER_KEEPER_STANDING_CLEARANCE,
    SOCCER_KEEPER_STANDING_BOTTOM       = SOCCER_GOAL_BOTTOM - SOCCER_KEEPER_STANDING_CLEARANCE,
    SOCCER_KEEPER_CATCH_RADIUS          = 14,
    SOCCER_KEEPER_TRACK_DISTANCE        = 420,
    SOCCER_KEEPER_DIVE_TRIGGER_DISTANCE = 120,
    SOCCER_KEEPER_DIVE_THRESHOLD        = 18,
    SOCCER_KEEPER_REACTION_TICKS        = 5,
    SOCCER_KEEPER_DIVE_SPEED            = 3,
    SOCCER_KEEPER_DIVE_TICKS            = 15,
    SOCCER_KEEPER_RECOVERY_TICKS        = 30,
    SOCCER_SHOT_BASE_SPEED              = 2304,
    SOCCER_SHOT_DIAGONAL_SPEED          = 1629,
    SOCCER_SHOT_MAX_CHARGE              = 30,
    SOCCER_SHOT_POWER_BASE              = 60,
    SOCCER_SHOT_HIGH_POWER_START        = 15,
    SOCCER_SHOT_MAX_PLACEMENT_ERROR     = 6
};
typedef enum {
    SOCCER_TITLE,
    SOCCER_PLAY,
    SOCCER_GOAL,
    SOCCER_KICKOFF,
    SOCCER_FULL_TIME,
    SOCCER_RESTART
} soccer_mode_t;
typedef enum {
    SOCCER_THROW_IN,
    SOCCER_CORNER,
    SOCCER_GOAL_KICK
} soccer_restart_t;
/* Ordered by priority when several actions happen on the same simulation tick. */
typedef enum {
    SOCCER_EVENT_NONE,
    SOCCER_EVENT_PASS,
    SOCCER_EVENT_SHOT,
    SOCCER_EVENT_HEADER,
    SOCCER_EVENT_TACKLE,
    SOCCER_EVENT_SAVE,
    SOCCER_EVENT_POST,
    SOCCER_EVENT_GOAL,
    SOCCER_EVENT_WHISTLE,
    SOCCER_EVENT_FINISH
} soccer_event_t;
typedef struct {
        int32_t x, y;
} soccer_vec_t;
typedef struct {
        int dx, dy;
        bool shoot, pass, switch_player, lob, shoot_held, through, sprint, pass_held;
} soccer_input_t;
typedef struct {
        soccer_vec_t position;
        int facing_x, facing_y;
        bool moving, sprinting;
        unsigned int stamina, sprint_rest;
        unsigned int kick_ticks, animation, tackle_cooldown;
        unsigned int jump_ticks, jump_cooldown;
        unsigned int slide_ticks;
        int slide_x, slide_y;
        bool slide_hit;
        bool headed;
} soccer_player_t;
typedef struct {
        unsigned int ticks; /* 45..31 committed dive, 30..1 recovery. */
        int direction;
        unsigned int reaction_ticks;
        bool observed;
} soccer_dive_t;
typedef enum {
    SOCCER_EASY,
    SOCCER_NORMAL,
    SOCCER_HARD
} soccer_difficulty_t;
typedef struct {
        uint32_t shots, on_target, saves, possession;
} soccer_stats_t;
typedef struct {
        soccer_difficulty_t difficulty;
        soccer_stats_t stats[2];
        unsigned int support_runner[2], support_ticks[2];
        soccer_vec_t support_destination[2];
        int shot_team;
        soccer_dive_t dives[2];
        soccer_player_t players[SOCCER_PLAYER_COUNT];
        soccer_player_t keepers[2];
        int keeper_owner;
        unsigned int keeper_ticks, match_ticks, kickoff_team;
        uint32_t opponent_goals;
        unsigned int controlled;
        unsigned int shot_charge, charge_player;
        unsigned int pass_charge, pass_player;
        unsigned int selection_candidate, selection_ticks, selection_cooldown;
        int owner, pass_target, last_kicker;
        soccer_vec_t ball, velocity, pass_destination;
        int32_t ball_height, vertical_velocity;
        bool paused, through_pass;
        unsigned int pickup_delay, goal_ticks, pass_ticks, possession_ticks;
        uint32_t goals, shots, passes, completed_passes, tackles, headers;
        soccer_mode_t mode;
        soccer_event_t event, feedback;
        unsigned int feedback_ticks;
        unsigned int last_touch_team, restart_team, restart_ticks;
        unsigned int restart_taker, restart_receiver;
        soccer_restart_t restart_kind;
} soccer_game_t;
void soccer_game_init(soccer_game_t* game);
void soccer_game_reset(soccer_game_t* game);
void soccer_game_arcade_step(soccer_game_t* game, soccer_input_t buttons);
void soccer_game_step(soccer_game_t* game, soccer_input_t input);
#endif
