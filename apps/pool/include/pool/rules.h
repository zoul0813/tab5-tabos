#ifndef POOL_RULES_H
#define POOL_RULES_H
#include <pool/physics.h>

typedef enum {
    POOL_OPEN,
    POOL_SOLIDS,
    POOL_STRIPES
} pool_group_t;
typedef enum {
    POOL_FAIR,
    POOL_SCRATCH,
    POOL_NO_CONTACT,
    POOL_WRONG_FIRST,
    POOL_NO_RAIL
} pool_foul_t;
typedef enum {
    POOL_RULE_IDLE,
    POOL_KEEP_TURN,
    POOL_CHANGE_TURN,
    POOL_BALL_IN_HAND,
    POOL_RERACK,
    POOL_RESULT
} pool_rule_result_t;

typedef struct {
        unsigned int player;
        pool_group_t groups[2];
        bool break_shot;
        bool pending;
        bool complete;
        unsigned int winner;
        bool early_eight;
        bool reracked;
        pool_foul_t foul;
        uint16_t before;
} pool_rules_t;

void pool_rules_reset(pool_rules_t* rules);
void pool_rules_begin(pool_rules_t* rules, uint16_t potted);
pool_rule_result_t pool_rules_finish(pool_rules_t* rules, uint16_t potted, const pool_pot_events_t* pots,
                                     const pool_contact_state_t* contacts);
unsigned int pool_rules_remaining(pool_group_t group, uint16_t potted);
bool pool_rules_legal_target(const pool_rules_t* rules, uint16_t potted, unsigned int ball);
const char* pool_rules_group_name(pool_group_t group);
const char* pool_rules_foul_name(pool_foul_t foul);
#endif
