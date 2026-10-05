#include <pool/rules.h>
#include <string.h>

static pool_group_t ball_group(unsigned int ball)
{
    if (ball >= 1U && ball <= 7U) {
        return POOL_SOLIDS;
    }
    if (ball >= 9U && ball <= 15U) {
        return POOL_STRIPES;
    }
    return POOL_OPEN;
}

unsigned int pool_rules_remaining(pool_group_t group, uint16_t potted)
{
    unsigned int left = 0U;
    for (unsigned int ball = 1U; ball < POOL_BALL_COUNT; ++ball) {
        if (ball_group(ball) == group && ball != 8U && (potted & (1U << ball)) == 0U) {
            ++left;
        }
    }
    return left;
}

void pool_rules_reset(pool_rules_t* rules)
{
    memset(rules, 0, sizeof(*rules));
    rules->break_shot = true;
}

bool pool_rules_legal_target(const pool_rules_t* rules, uint16_t potted, unsigned int ball)
{
    if (rules->complete || ball == 0U || ball >= POOL_BALL_COUNT || (potted & (1U << ball)) != 0U) {
        return false;
    }
    const pool_group_t group = rules->groups[rules->player];
    if (group == POOL_OPEN) {
        return ball != 8U;
    }
    if (pool_rules_remaining(group, potted) == 0U) {
        return ball == 8U;
    }
    return ball_group(ball) == group;
}

void pool_rules_begin(pool_rules_t* rules, uint16_t potted)
{
    if (!rules->complete && !rules->pending) {
        rules->before   = potted;
        rules->pending  = true;
        rules->foul     = POOL_FAIR;
        rules->reracked = false;
    }
}

pool_rule_result_t pool_rules_finish(pool_rules_t* rules, uint16_t potted, const pool_pot_events_t* pots,
                                     const pool_contact_state_t* contacts)
{
    if (!rules->pending || rules->complete) {
        return POOL_RULE_IDLE;
    }
    rules->pending       = false;
    const uint16_t newly = potted & (uint16_t) ~rules->before;
    const bool eight     = (newly & (1U << 8U)) != 0U;
    const bool was_break = rules->break_shot;
    if (was_break && eight) {
        const unsigned int breaker = rules->player;
        pool_rules_reset(rules);
        rules->player   = breaker;
        rules->reracked = true;
        return POOL_RERACK;
    }
    if ((newly & 1U) != 0U) {
        rules->foul = POOL_SCRATCH;
    } else if (contacts->first == 0U) {
        rules->foul = POOL_NO_CONTACT;
    } else if (!pool_rules_legal_target(rules, rules->before, contacts->first)) {
        rules->foul = POOL_WRONG_FIRST;
    } else if (!contacts->rail_after && (newly & UINT16_C(0xfffe)) == 0U) {
        rules->foul = POOL_NO_RAIL;
    }
    rules->break_shot = false;
    if (eight) {
        const pool_group_t group = rules->groups[rules->player];
        rules->early_eight       = group == POOL_OPEN || pool_rules_remaining(group, rules->before) != 0U;
        rules->complete          = true;
        rules->winner            = rules->player;
        if (rules->early_eight || rules->foul != POOL_FAIR) {
            rules->winner ^= 1U;
        }
        return POOL_RESULT;
    }
    if (rules->foul != POOL_FAIR) {
        rules->player ^= 1U;
        return POOL_BALL_IN_HAND;
    }
    if (!was_break && rules->groups[rules->player] == POOL_OPEN) {
        for (unsigned int i = 0U; i < pots->count; ++i) {
            const pool_group_t group = ball_group(pots->ball[i]);
            if (group != POOL_OPEN && (newly & (1U << pots->ball[i])) != 0U) {
                rules->groups[rules->player]      = group;
                rules->groups[rules->player ^ 1U] = group == POOL_SOLIDS ? POOL_STRIPES : POOL_SOLIDS;
                break;
            }
        }
    }
    const pool_group_t group = rules->groups[rules->player];
    for (unsigned int ball = 1U; ball < POOL_BALL_COUNT; ++ball) {
        if (ball != 8U && (newly & (1U << ball)) != 0U && (was_break || ball_group(ball) == group)) {
            return POOL_KEEP_TURN;
        }
    }
    rules->player ^= 1U;
    return POOL_CHANGE_TURN;
}

const char* pool_rules_group_name(pool_group_t group)
{
    switch (group) {
        case POOL_SOLIDS: return "SOLIDS";
        case POOL_STRIPES: return "STRIPES";
        default: return "OPEN";
    }
}

const char* pool_rules_foul_name(pool_foul_t foul)
{
    switch (foul) {
        case POOL_SCRATCH: return "SCRATCH";
        case POOL_NO_CONTACT: return "NO CONTACT";
        case POOL_WRONG_FIRST: return "WRONG FIRST BALL";
        case POOL_NO_RAIL: return "NO RAIL OR POT";
        default: return "LEGAL SHOT";
    }
}
