#include <soccer/game.h>
#include <string.h>
#include <stdlib.h>

static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void action(soccer_game_t* game, soccer_event_t event)
{
    if (event > game->event) {
        game->event          = event;
        game->feedback       = event;
        game->feedback_ticks = 30U;
    }
}

static void kickoff(soccer_game_t* game, unsigned int team)
{
    memset(game->players, 0, sizeof(game->players));
    memset(game->keepers, 0, sizeof(game->keepers));
    memset(game->dives, 0, sizeof(game->dives));
    game->shot_charge         = 0U;
    game->shot_team           = -1;
    static const int home_x[] = {650, 650, 650, 340, 340};
    static const int home_y[] = {480, 320, 640, 330, 630};
    _Static_assert(sizeof(home_x) / sizeof(home_x[0]) == SOCCER_TEAM_SIZE, "kickoff squad size");
    _Static_assert(sizeof(home_y) / sizeof(home_y[0]) == SOCCER_TEAM_SIZE, "kickoff lanes");
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        const unsigned int side   = i / SOCCER_TEAM_SIZE;
        const unsigned int role   = i % SOCCER_TEAM_SIZE;
        const int x               = side == 0U ? home_x[role] : 1600 - home_x[role];
        game->players[i].position = (soccer_vec_t) {x * SOCCER_ONE, home_y[role] * SOCCER_ONE};
        game->players[i].facing_x = side == 0U ? 1 : -1;
        game->players[i].stamina  = 90U;
    }
    game->players[team * SOCCER_TEAM_SIZE].position =
        (soccer_vec_t) {(team == 0U ? 784 : 816) * SOCCER_ONE, 480 * SOCCER_ONE};
    game->keepers[0].position = (soccer_vec_t) {(SOCCER_LEFT + 22) * SOCCER_ONE, SOCCER_PITCH_CENTRE_Y * SOCCER_ONE};
    game->keepers[1].position = (soccer_vec_t) {(SOCCER_RIGHT - 22) * SOCCER_ONE, SOCCER_PITCH_CENTRE_Y * SOCCER_ONE};
    game->keepers[0].facing_x = 1;
    game->keepers[1].facing_x = -1;
    game->ball                = (soccer_vec_t) {800 * SOCCER_ONE, SOCCER_PITCH_CENTRE_Y * SOCCER_ONE};
    game->velocity            = (soccer_vec_t) {0, 0};
    game->ball_height         = 0;
    game->vertical_velocity   = 0;
    game->controlled          = 0U;
    game->owner               = (int) team * SOCCER_TEAM_SIZE;
    game->keeper_owner        = -1;
    game->keeper_ticks        = 0U;
    game->kickoff_team        = team;
    game->last_touch_team     = team;
    game->pass_target         = -1;
    game->through_pass        = false;
    game->last_kicker         = -1;
    game->pickup_delay        = 0U;
    game->goal_ticks          = 90U;
    game->pass_ticks          = 0U;
    game->possession_ticks    = 60U;
    game->pass_destination    = (soccer_vec_t) {0, 0};
    game->feedback_ticks      = 0U;
    game->paused              = false;
    game->mode                = SOCCER_KICKOFF;
}

void soccer_game_reset(soccer_game_t* game)
{
    memset(game, 0, sizeof(*game));
    game->match_ticks = SOCCER_MATCH_TICKS;
    game->difficulty  = SOCCER_NORMAL;
    kickoff(game, 0U);
}

void soccer_game_init(soccer_game_t* game)
{
    memset(game, 0, sizeof(*game));
    soccer_game_reset(game);
    game->mode = SOCCER_TITLE;
}

static int32_t distance_squared(soccer_vec_t a, soccer_vec_t b)
{
    const int32_t dx = (a.x - b.x) / SOCCER_ONE;
    const int32_t dy = (a.y - b.y) / SOCCER_ONE;
    return dx * dx + dy * dy;
}

static uint32_t square_root(uint32_t value)
{
    uint32_t low = 0U, high = 4096U;
    while (low + 1U < high) {
        const uint32_t middle = (low + high) / 2U;
        if (middle * middle <= value) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

static void move_player(soccer_player_t* player, int dx, int dy)
{
    player->moving = dx != 0 || dy != 0;
    if (player->moving) {
        player->facing_x = dx;
        player->facing_y = dy;
        int speed        = dx != 0 && dy != 0 ? 452 : 640;
        if (player->sprinting) {
            speed = dx != 0 && dy != 0 ? 633 : 896;
        }
        player->position.x += dx * speed;
        player->position.y += dy * speed;
        player->animation  += player->sprinting ? 2U : 1U;
    }
    player->position.x = clamp(player->position.x, (SOCCER_LEFT + 8) * SOCCER_ONE, (SOCCER_RIGHT - 8) * SOCCER_ONE);
    player->position.y = clamp(player->position.y, (SOCCER_TOP + 8) * SOCCER_ONE, (SOCCER_BOTTOM - 8) * SOCCER_ONE);
}

static unsigned int nearest_player(const soccer_game_t* game, unsigned int team, int excluded)
{
    unsigned int best = team * SOCCER_TEAM_SIZE;
    int32_t distance  = INT32_MAX;
    for (unsigned int i = team * SOCCER_TEAM_SIZE; i < (team + 1U) * SOCCER_TEAM_SIZE; ++i) {
        const int32_t candidate = distance_squared(game->players[i].position, game->ball);
        if ((int) i != excluded && candidate < distance) {
            best     = i;
            distance = candidate;
        }
    }
    return best;
}

/* A recovering AI presser can hand pressure to a ready nearby teammate.
   Never reinterpret the human player's movement as an AI assignment. */
static unsigned int pressing_player(const soccer_game_t* game, unsigned int team)
{
    unsigned int best = nearest_player(game, team, -1);
    if (best == game->controlled || game->owner < 0 || (unsigned int) game->owner / SOCCER_TEAM_SIZE == team ||
        game->players[best].tackle_cooldown == 0U) {
        return best;
    }
    int32_t distance = 120 * 120;
    for (unsigned int i = team * SOCCER_TEAM_SIZE; i < (team + 1U) * SOCCER_TEAM_SIZE; ++i) {
        const soccer_player_t* player = &game->players[i];
        const int32_t candidate       = distance_squared(player->position, game->ball);
        if (i != game->controlled && player->tackle_cooldown == 0U && player->jump_ticks == 0U &&
            candidate < distance) {
            best     = i;
            distance = candidate;
        }
    }
    return best;
}

/* Manual defensive switching prefers someone who can immediately control the ball.
   Keep the fallback so repeated switching always reaches another outfield player. */
static unsigned int defensive_selection(const soccer_game_t* game)
{
    unsigned int best = nearest_player(game, 0U, (int) game->controlled);
    int32_t distance  = INT32_MAX;
    for (unsigned int i = 0U; i < SOCCER_TEAM_SIZE; ++i) {
        const soccer_player_t* player = &game->players[i];
        if (i == game->controlled || player->jump_ticks > 0U || player->tackle_cooldown > 24U) {
            continue;
        }
        const int32_t candidate = distance_squared(player->position, game->ball);
        if (candidate < distance) {
            best     = i;
            distance = candidate;
        }
    }
    return best;
}

static void steer(soccer_player_t* player, soccer_vec_t target, bool opponent, soccer_difficulty_t difficulty)
{
    target.x         = clamp(target.x, (SOCCER_LEFT + 16) * SOCCER_ONE, (SOCCER_RIGHT - 16) * SOCCER_ONE);
    target.y         = clamp(target.y, (SOCCER_TOP + 16) * SOCCER_ONE, (SOCCER_BOTTOM - 16) * SOCCER_ONE);
    const int32_t dx = target.x - player->position.x, dy = target.y - player->position.y;
    int x = 0, y = 0;
    if (abs(dx) > 4 * SOCCER_ONE) {
        x = dx > 0 ? 1 : -1;
    }
    if (abs(dy) > 4 * SOCCER_ONE) {
        y = dy > 0 ? 1 : -1;
    }
    const soccer_vec_t previous = player->position;
    move_player(player, x, y);
    if (opponent) {
        int numerator = 2, divisor = 3;
        if (difficulty == SOCCER_EASY || player->tackle_cooldown > 24U) {
            numerator = 1;
            divisor   = 2;
        } else if (difficulty == SOCCER_HARD) {
            numerator = 3;
            divisor   = 4;
        }
        player->position.x = previous.x + (player->position.x - previous.x) * numerator / divisor;
        player->position.y = previous.y + (player->position.y - previous.y) * numerator / divisor;
    }
}

/* 2-2-1: one striker, two midfielders and two defenders. Limit the block's
   advance so the whole team does not collapse onto the goal line. */
static soccer_vec_t formation(const soccer_game_t* game, unsigned int index, bool attacking)
{
    const unsigned int team = index / SOCCER_TEAM_SIZE, role = index % SOCCER_TEAM_SIZE;
    const int direction        = team == 0U ? 1 : -1;
    static const int lanes[]   = {480, 300, 660, 370, 590};
    static const int advance[] = {160, -60, -60, -260, -260};
    _Static_assert(sizeof(lanes) / sizeof(lanes[0]) == SOCCER_TEAM_SIZE, "formation lanes");
    _Static_assert(sizeof(advance) / sizeof(advance[0]) == SOCCER_TEAM_SIZE, "formation depth");
    const int32_t anchor = clamp(game->ball.x, 400 * SOCCER_ONE, 1200 * SOCCER_ONE);
    int32_t x            = anchor + direction * advance[role] * SOCCER_ONE;
    if (!attacking) {
        const int goal      = team == 0U ? SOCCER_LEFT : SOCCER_RIGHT;
        const int32_t block = (game->ball.x + goal * SOCCER_ONE) / 2;
        x                   = block + direction * advance[role] * SOCCER_ONE / 2;
    }
    return (soccer_vec_t) {x, (lanes[role] * SOCCER_ONE * 3 + game->ball.y) / 4};
}

/* Score the actual passing corridor and its receiving end in world units. */
static int lane_pressure(const soccer_game_t* game, unsigned int team, soccer_vec_t target)
{
    const int32_t dx      = (target.x - game->ball.x) / SOCCER_ONE;
    const int32_t dy      = (target.y - game->ball.y) / SOCCER_ONE;
    const int32_t squared = dx * dx + dy * dy;
    int pressure          = 0;
    for (unsigned int enemy = (1U - team) * SOCCER_TEAM_SIZE; enemy < (2U - team) * SOCCER_TEAM_SIZE; ++enemy) {
        const soccer_vec_t position = game->players[enemy].position;
        const int32_t ex            = (position.x - game->ball.x) / SOCCER_ONE;
        const int32_t ey            = (position.y - game->ball.y) / SOCCER_ONE;
        const int32_t dot           = ex * dx + ey * dy;
        const int32_t cross         = ex * dy - ey * dx;
        if (dot > 0 && dot < squared && (int64_t) cross * cross < (int64_t) 24 * 24 * squared) {
            pressure += 400;
        }
        if (distance_squared(position, target) < 40 * 40) {
            pressure += 200;
        }
    }
    return pressure;
}

/* Look only a short stride ahead so dribbling avoids nearby defenders
   without abandoning the attack or searching the whole pitch. */
static soccer_vec_t opponent_dribble_target(const soccer_game_t* game, unsigned int index)
{
    const soccer_vec_t position = game->players[index].position;
    soccer_vec_t target         = {(SOCCER_LEFT + 80) * SOCCER_ONE,
                                   ((index % SOCCER_TEAM_SIZE) % 2U == 0U ? 458 : 502) * SOCCER_ONE};
    soccer_vec_t ahead          = {position.x - 64 * SOCCER_ONE, position.y};
    if (position.x <= (SOCCER_LEFT + 96) * SOCCER_ONE) {
        return target;
    }
    if (lane_pressure(game, 1U, ahead) == 0) {
        return target;
    }
    for (int side = -1; side <= 1; side += 2) {
        soccer_vec_t candidate = {ahead.x, position.y + side * 56 * SOCCER_ONE};
        if (candidate.y < (SOCCER_TOP + 24) * SOCCER_ONE || candidate.y > (SOCCER_BOTTOM - 24) * SOCCER_ONE) {
            continue;
        }
        if (lane_pressure(game, 1U, candidate) == 0) {
            return candidate;
        }
    }
    return target;
}

/* Keep the role's depth, but offer a nearby open lane instead of waiting behind a marker. */
static soccer_vec_t attacking_support(const soccer_game_t* game, unsigned int index, soccer_vec_t home)
{
    if (index % SOCCER_TEAM_SIZE >= SOCCER_DEFENDER_FIRST || distance_squared(home, game->ball) >= 280 * 280) {
        return home;
    }
    const unsigned int team = index / SOCCER_TEAM_SIZE;
    soccer_vec_t best       = home;
    int best_score          = lane_pressure(game, team, home);
    for (int side = -1; side <= 1; side += 2) {
        soccer_vec_t candidate = home;
        candidate.y =
            clamp(home.y + side * 72 * SOCCER_ONE, (SOCCER_TOP + 32) * SOCCER_ONE, (SOCCER_BOTTOM - 32) * SOCCER_ONE);
        if (distance_squared(candidate, game->ball) >= 280 * 280) {
            continue;
        }
        /* Prefer the normal formation when both routes are open. */
        int score = 50 + lane_pressure(game, team, candidate);
        for (unsigned int other = team * SOCCER_TEAM_SIZE; other < (team + 1U) * SOCCER_TEAM_SIZE; ++other) {
            if (other != index && distance_squared(candidate, game->players[other].position) < 50 * 50) {
                score += 200;
            }
        }
        if (score < best_score) {
            best       = candidate;
            best_score = score;
        }
    }
    return best;
}

/* Assign distinct nearby threats to the two defenders. The presser and human player
   retain their own jobs; defenders stay between their mark and the goal. */
static void defensive_cover(const soccer_game_t* game, unsigned int team, unsigned int chaser,
                            soccer_vec_t targets[SOCCER_TEAM_SIZE])
{
    bool assigned[SOCCER_TEAM_SIZE] = {false};
    bool marked[SOCCER_TEAM_SIZE]   = {false};
    const int direction             = team == 0U ? 1 : -1;
    const int goal                  = team == 0U ? SOCCER_LEFT : SOCCER_RIGHT;
    for (unsigned int role = 0U; role < SOCCER_TEAM_SIZE; ++role) {
        targets[role] = formation(game, team * SOCCER_TEAM_SIZE + role, false);
    }
    if (game->keeper_owner >= 0 || (game->owner < 0 && game->pass_target < 0)) {
        return;
    }
    /* Globally choose the closest formation/attacker pair, rather than letting
       the first defender claim a threat better covered by the other defender. */
    for (unsigned int slot = 0U; slot < SOCCER_TEAM_SIZE - SOCCER_DEFENDER_FIRST; ++slot) {
        int defender = -1, attacker = -1;
        int32_t best = 240 * 240;
        for (unsigned int role = SOCCER_DEFENDER_FIRST; role < SOCCER_TEAM_SIZE; ++role) {
            const unsigned int index = team * SOCCER_TEAM_SIZE + role;
            if (assigned[role] || index == chaser || index == game->controlled) {
                continue;
            }
            for (unsigned int other = 0U; other < SOCCER_TEAM_SIZE; ++other) {
                const unsigned int enemy    = (1U - team) * SOCCER_TEAM_SIZE + other;
                const soccer_vec_t position = game->players[enemy].position;
                const int32_t depth         = (position.x / SOCCER_ONE - goal) * direction;
                if (marked[other] || game->owner == (int) enemy || depth > 650 ||
                    (position.x - game->ball.x) * direction > 120 * SOCCER_ONE) {
                    continue;
                }
                const int32_t distance = distance_squared(targets[role], position);
                if (distance < best) {
                    best     = distance;
                    defender = (int) role;
                    attacker = (int) other;
                }
            }
        }
        if (defender < 0) {
            break;
        }
        assigned[defender]   = true;
        marked[attacker]     = true;
        targets[defender]    = game->players[(1U - team) * SOCCER_TEAM_SIZE + (unsigned int) attacker].position;
        targets[defender].x -= direction * 32 * SOCCER_ONE;
    }
}

/* Tackles require a fresh attempt, proximity and facing the carrier. A miss
   leaves recovery time; a won ball receives a short protection window. */
static void tackle_contact(soccer_game_t* game, unsigned int index)
{
    soccer_player_t* player = &game->players[index];
    if (player->slide_hit || game->ball_height > 4 * SOCCER_ONE) {
        return;
    }
    if (game->owner < 0 || (unsigned int) game->owner / SOCCER_TEAM_SIZE == index / SOCCER_TEAM_SIZE ||
        game->possession_ticks > 0U) {
        return;
    }
    soccer_player_t* carrier = &game->players[game->owner];
    const int32_t dx         = (carrier->position.x - player->position.x) / SOCCER_ONE;
    const int32_t dy         = (carrier->position.y - player->position.y) / SOCCER_ONE;
    if (dx * dx + dy * dy > 30 * 30 || dx * player->facing_x + dy * player->facing_y < 0) {
        return;
    }
    player->slide_hit     = true;
    game->last_touch_team = index / SOCCER_TEAM_SIZE;
    action(game, SOCCER_EVENT_TACKLE);
    carrier->tackle_cooldown = 42U;
    game->owner              = (int) index;
    game->possession_ticks   = 30U;
    game->pass_target        = -1;
    game->through_pass       = false;
    game->pass_ticks         = 0U;
    game->velocity           = (soccer_vec_t) {0, 0};
    if (index < SOCCER_TEAM_SIZE) {
        /* A tackle is a possession recovery too: never leave a blue AI carrier
           waiting for input while the human still controls another teammate. */
        game->controlled  = index;
        game->shot_charge = 0U;
        game->pass_charge = 0U;
        ++game->tackles;
    }
}

static void tackle(soccer_game_t* game, unsigned int index)
{
    soccer_player_t* player = &game->players[index];
    if (player->tackle_cooldown != 0U || player->jump_ticks > 0U || player->slide_ticks > 0U) {
        return;
    }
    player->tackle_cooldown = 42U;
    player->kick_ticks      = 14U;
    player->slide_ticks     = 18U;
    player->slide_x         = player->facing_x;
    player->slide_y         = player->facing_y;
    if (player->slide_x == 0 && player->slide_y == 0) {
        player->slide_x = index < SOCCER_TEAM_SIZE ? 1 : -1;
    }
    player->slide_hit = false;
    tackle_contact(game, index);
}

static void slide_step(soccer_game_t* game, unsigned int index)
{
    soccer_player_t* player = &game->players[index];
    if (player->slide_ticks == 0U) {
        return;
    }
    const int speed    = (int) ((player->slide_ticks + 5U) / 6U) * SOCCER_ONE;
    const int stride   = player->slide_x != 0 && player->slide_y != 0 ? speed * 181 / 256 : speed;
    player->position.x = clamp(player->position.x + player->slide_x * stride, (SOCCER_LEFT + 8) * SOCCER_ONE,
                               (SOCCER_RIGHT - 8) * SOCCER_ONE);
    player->position.y = clamp(player->position.y + player->slide_y * stride, (SOCCER_TOP + 8) * SOCCER_ONE,
                               (SOCCER_BOTTOM - 8) * SOCCER_ONE);
    player->facing_x   = player->slide_x;
    player->facing_y   = player->slide_y;
    player->moving     = true;
    tackle_contact(game, index);
    --player->slide_ticks;
}

static void jump(soccer_player_t* player)
{
    if (player->jump_cooldown == 0U && player->tackle_cooldown == 0U) {
        player->jump_ticks    = 24U;
        player->jump_cooldown = 54U;
        player->headed        = false;
    }
}

static void headers(soccer_game_t* game)
{
    if (game->owner >= 0 || game->keeper_owner >= 0 || game->ball_height < 12 * SOCCER_ONE ||
        game->ball_height > 44 * SOCCER_ONE) {
        return;
    }
    /* Closest eligible jumper wins the challenge; ties favour the controlled
       player rather than always privileging the lowest squad index. */
    int winner      = -1;
    int32_t closest = 24 * 24 + 1;
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        const soccer_player_t* player = &game->players[i];
        if (player->jump_ticks < 6U || player->jump_ticks > 18U || player->headed) {
            continue;
        }
        const int32_t distance = distance_squared(player->position, game->ball);
        if (distance < closest || (distance == closest && i == game->controlled)) {
            winner  = (int) i;
            closest = distance;
        }
    }
    if (winner < 0 || closest > 24 * 24) {
        return;
    }
    soccer_player_t* player = &game->players[winner];
    player->headed          = true;
    game->shot_team         = winner / SOCCER_TEAM_SIZE;
    ++game->stats[game->shot_team].shots;
    const int speed         = player->facing_x != 0 && player->facing_y != 0 ? 1267 : 1792;
    game->velocity          = (soccer_vec_t) {player->facing_x * speed, player->facing_y * speed};
    game->vertical_velocity = -2 * SOCCER_ONE;
    game->last_touch_team   = (unsigned int) winner / SOCCER_TEAM_SIZE;
    game->last_kicker       = winner;
    game->pickup_delay      = 18U;
    game->pass_target       = -1;
    game->through_pass      = false;
    game->pass_ticks        = 0U;
    if ((unsigned int) winner < SOCCER_TEAM_SIZE) {
        ++game->headers;
    }
    action(game, SOCCER_EVENT_HEADER);
}

/* Opposing off-ball formations can converge on the same spot. Resolve their
   ground overlap gently after steering; never displace the human or a ball duel. */
static void separate_off_ball_players(soccer_game_t* game)
{
    for (unsigned int blue = 0U; blue < SOCCER_TEAM_SIZE; ++blue) {
        for (unsigned int red = SOCCER_TEAM_SIZE; red < SOCCER_PLAYER_COUNT; ++red) {
            soccer_player_t* first  = &game->players[blue];
            soccer_player_t* second = &game->players[red];
            if (first->jump_ticks > 0U || second->jump_ticks > 0U ||
                distance_squared(first->position, game->ball) < 80 * 80 ||
                distance_squared(second->position, game->ball) < 80 * 80 ||
                distance_squared(first->position, second->position) >= 24 * 24) {
                continue;
            }
            const int32_t dx         = second->position.x - first->position.x;
            const int32_t dy         = second->position.y - first->position.y;
            const bool horizontal    = abs(dx) > abs(dy);
            const int32_t gap        = horizontal ? abs(dx) : abs(dy);
            const int direction      = (horizontal ? dx : dy) < 0 ? -1 : 1;
            const int32_t correction = clamp((24 * SOCCER_ONE - gap) / 2, 0, 3 * SOCCER_ONE);
            if (blue != game->controlled) {
                if (horizontal) {
                    first->position.x -= direction * correction;
                } else {
                    first->position.y -= direction * correction;
                }
                first->position.x =
                    clamp(first->position.x, (SOCCER_LEFT + 8) * SOCCER_ONE, (SOCCER_RIGHT - 8) * SOCCER_ONE);
                first->position.y =
                    clamp(first->position.y, (SOCCER_TOP + 8) * SOCCER_ONE, (SOCCER_BOTTOM - 8) * SOCCER_ONE);
            }
            if (horizontal) {
                second->position.x += direction * correction;
            } else {
                second->position.y += direction * correction;
            }
            second->position.x =
                clamp(second->position.x, (SOCCER_LEFT + 8) * SOCCER_ONE, (SOCCER_RIGHT - 8) * SOCCER_ONE);
            second->position.y =
                clamp(second->position.y, (SOCCER_TOP + 8) * SOCCER_ONE, (SOCCER_BOTTOM - 8) * SOCCER_ONE);
        }
    }
}

static void teams(soccer_game_t* game, const bool slid[SOCCER_PLAYER_COUNT])
{
    for (unsigned int team = 0U; team < 2U; ++team) {
        if (game->support_ticks[team] > 0U) {
            --game->support_ticks[team];
            if ((game->owner >= 0 && (unsigned int) game->owner / SOCCER_TEAM_SIZE != team) ||
                game->keeper_owner >= 0) {
                game->support_ticks[team] = 0U;
            }
        }
        const unsigned int chaser = pressing_player(game, team);
        bool attacking            = false;
        if (game->owner >= 0) {
            attacking = (unsigned int) game->owner / SOCCER_TEAM_SIZE == team;
        } else if (game->keeper_owner >= 0) {
            attacking = (unsigned int) game->keeper_owner == team;
        } else if (game->pass_target >= 0 && game->pass_ticks > 0U) {
            attacking = (unsigned int) game->pass_target / SOCCER_TEAM_SIZE == team;
        }
        soccer_vec_t cover[SOCCER_TEAM_SIZE];
        if (!attacking) {
            defensive_cover(game, team, chaser, cover);
        }
        for (unsigned int i = team * SOCCER_TEAM_SIZE; i < (team + 1U) * SOCCER_TEAM_SIZE; ++i) {
            if (i == game->controlled || slid[i] || game->players[i].slide_ticks > 0U) {
                continue;
            }
            soccer_player_t* player = &game->players[i];
            soccer_vec_t target     = attacking ? formation(game, i, true) : cover[i % SOCCER_TEAM_SIZE];
            if (attacking && game->owner >= 0 && game->owner != (int) i) {
                target = attacking_support(game, i, target);
            }
            if (attacking && game->support_ticks[team] > 0U && game->support_runner[team] == i &&
                lane_pressure(game, team, game->support_destination[team]) == 0) {
                target = game->support_destination[team];
            }
            if (game->owner == (int) i) {
                if (team == 0U) {
                    player->moving = false;
                    continue;
                }
                target = opponent_dribble_target(game, i);
            } else if (game->pass_target == (int) i) {
                target = game->pass_destination;
                if (!game->through_pass && distance_squared(player->position, game->ball) < 70 * 70) {
                    target = game->ball;
                }
            } else if (!attacking && i == chaser && game->keeper_owner < 0) {
                target    = game->ball;
                target.x += game->velocity.x * 8;
                target.y += game->velocity.y * 8;
            }
            /* Short local avoidance keeps supporting runs from stacking on a carrier. */
            for (unsigned int other = team * SOCCER_TEAM_SIZE; other < (team + 1U) * SOCCER_TEAM_SIZE; ++other) {
                if (other != i && distance_squared(player->position, game->players[other].position) < 28 * 28) {
                    target.y += (i < other ? -40 : 40) * SOCCER_ONE;
                }
            }
            steer(player, target, team == 1U, game->difficulty);
            if (i == chaser && game->owner < 0 && game->keeper_owner < 0 && game->last_touch_team != team &&
                game->ball_height >= 12 * SOCCER_ONE && game->ball_height <= 44 * SOCCER_ONE &&
                distance_squared(player->position, game->ball) <= 36 * 36) {
                jump(player);
                player->facing_x = team == 0U ? 1 : -1;
                player->facing_y = 0;
            }
            if (!attacking && i == chaser && game->owner >= 0 && game->possession_ticks == 0U &&
                distance_squared(player->position, game->players[game->owner].position) <= 26 * 26) {
                /* Reaching the ball can stop steering with the carrier behind
                   us. Face the actual opponent before the AI's tackle attempt. */
                const soccer_vec_t carrier = game->players[game->owner].position;
                const int32_t dx           = carrier.x - player->position.x;
                const int32_t dy           = carrier.y - player->position.y;
                if (dx != 0 || dy != 0) {
                    player->facing_x = (dx > 0) - (dx < 0);
                    player->facing_y = (dy > 0) - (dy < 0);
                }
                tackle(game, i);
            }
        }
    }
}

static void collect(soccer_game_t* game)
{
    if (game->owner >= 0 || game->keeper_owner >= 0 || game->ball_height > 4 * SOCCER_ONE) {
        return;
    }
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        if ((game->last_kicker == (int) i && game->pickup_delay > 0U) ||
            (i < SOCCER_TEAM_SIZE && i != game->controlled && game->pass_target != (int) i && game->last_kicker >= 0 &&
             game->last_kicker < SOCCER_TEAM_SIZE && abs(game->velocity.x) + abs(game->velocity.y) > SOCCER_ONE) ||
            game->players[i].tackle_cooldown > 24U || game->players[i].jump_ticks > 0U) {
            continue;
        }
        if (distance_squared(game->players[i].position, game->ball) <= 18 * 18) {
            game->last_touch_team   = i / SOCCER_TEAM_SIZE;
            game->owner             = (int) i;
            game->shot_team         = -1;
            game->possession_ticks  = 24U;
            game->velocity          = (soccer_vec_t) {0, 0};
            game->ball_height       = 0;
            game->vertical_velocity = 0;
            if (i < SOCCER_TEAM_SIZE) {
                if (game->pass_target == (int) i) {
                    ++game->completed_passes;
                }
                game->controlled = i;
            }
            game->pass_target  = -1;
            game->through_pass = false;
            game->pass_ticks   = 0U;
            return;
        }
    }
}

/* Prefer reachable receivers along the intended direction; penalize lanes
   with a defender near the segment so assistance does not force a marked pass. */
static int pass_receiver(const soccer_game_t* game, unsigned int kicker)
{
    const unsigned int team       = kicker / SOCCER_TEAM_SIZE;
    const soccer_player_t* player = &game->players[kicker];
    int best                      = -1;
    int32_t best_score            = INT32_MAX;
    for (unsigned int i = team * SOCCER_TEAM_SIZE; i < (team + 1U) * SOCCER_TEAM_SIZE; ++i) {
        if (i == kicker || game->players[i].tackle_cooldown > 24U || game->players[i].jump_ticks > 0U) {
            continue;
        }
        const int32_t dx      = (game->players[i].position.x - game->ball.x) / SOCCER_ONE;
        const int32_t dy      = (game->players[i].position.y - game->ball.y) / SOCCER_ONE;
        const int32_t squared = dx * dx + dy * dy;
        if (squared < 20 * 20 || squared >= 280 * 280) {
            continue;
        }
        if (team == 0U && kicker != game->controlled) {
            if (i == game->controlled) {
                return (int) i;
            }
            continue;
        }
        const int32_t forward = dx * player->facing_x + dy * player->facing_y;
        if (forward <= 0) {
            continue;
        }
        int32_t score  = (int32_t) square_root((uint32_t) squared) + abs(dx * player->facing_y - dy * player->facing_x);
        score         += lane_pressure(game, team, game->players[i].position);
        if (score < best_score) {
            best_score = score;
            best       = (int) i;
        }
    }
    return best;
}

static void kick(soccer_game_t* game, bool pass)
{
    action(game, pass ? SOCCER_EVENT_PASS : SOCCER_EVENT_SHOT);
    const unsigned int kicker = (unsigned int) game->owner;
    game->shot_team           = pass ? -1 : (int) (kicker / SOCCER_TEAM_SIZE);
    if (!pass) {
        ++game->stats[kicker / SOCCER_TEAM_SIZE].shots;
    }
    game->last_touch_team   = kicker / SOCCER_TEAM_SIZE;
    soccer_player_t* player = &game->players[kicker];
    const int speed =
        player->facing_x != 0 && player->facing_y != 0 ? SOCCER_SHOT_DIAGONAL_SPEED : SOCCER_SHOT_BASE_SPEED;
    game->velocity     = (soccer_vec_t) {player->facing_x * speed, player->facing_y * speed};
    game->pass_target  = -1;
    game->through_pass = false;
    game->pass_ticks   = 0U;
    if (pass) {
        const int receiver = pass_receiver(game, kicker);
        if (receiver >= 0) {
            const soccer_vec_t target = game->players[receiver].position;
            const int32_t dx          = (target.x - game->ball.x) / SOCCER_ONE;
            const int32_t dy          = (target.y - game->ball.y) / SOCCER_ONE;
            const uint32_t distance   = square_root((uint32_t) (dx * dx + dy * dy));
            game->velocity            = (soccer_vec_t) {dx * 1920 / (int32_t) distance, dy * 1920 / (int32_t) distance};
            game->pass_target         = receiver;
            game->pass_destination    = target;
            game->pass_ticks          = 120U;
            const unsigned int team   = kicker / SOCCER_TEAM_SIZE;
            game->support_ticks[team] = 0U;
            if (kicker % SOCCER_TEAM_SIZE < SOCCER_DEFENDER_FIRST) {
                const int direction        = team == 0U ? 1 : -1;
                game->support_runner[team] = kicker;
                game->support_ticks[team]  = 150U;
                game->support_destination[team] =
                    (soccer_vec_t) {clamp(player->position.x + direction * 120 * SOCCER_ONE,
                                          (SOCCER_LEFT + 80) * SOCCER_ONE, (SOCCER_RIGHT - 80) * SOCCER_ONE),
                                    clamp(player->position.y + (target.y < player->position.y ? 64 : -64) * SOCCER_ONE,
                                          (SOCCER_TOP + 48) * SOCCER_ONE, (SOCCER_BOTTOM - 48) * SOCCER_ONE)};
            }
        } else {
            game->velocity.x = game->velocity.x * 2 / 3;
            game->velocity.y = game->velocity.y * 2 / 3;
        }
        if (kicker < SOCCER_TEAM_SIZE) {
            ++game->passes;
        }
    } else {
        if (kicker < SOCCER_TEAM_SIZE) {
            ++game->shots;
        }
    }
    player->kick_ticks = 10U;
    game->owner        = -1;
    game->last_kicker  = (int) kicker;
    game->pickup_delay = 18U;
}

static void aim_ball(soccer_game_t* game, soccer_vec_t target, int speed)
{
    const int32_t dx        = (target.x - game->ball.x) / SOCCER_ONE;
    const int32_t dy        = (target.y - game->ball.y) / SOCCER_ONE;
    const uint32_t distance = square_root((uint32_t) (dx * dx + dy * dy));
    if (distance > 0U) {
        game->velocity = (soccer_vec_t) {dx * speed / (int32_t) distance, dy * speed / (int32_t) distance};
    }
}

static bool human_shot_target(const soccer_game_t* game, unsigned int kicker, soccer_input_t input, unsigned int power,
                              soccer_vec_t* target)
{
    const soccer_player_t* player = &game->players[kicker];
    int aim_x                     = input.dx;
    int aim_y                     = input.dy;
    if (aim_x == 0 && aim_y == 0) {
        aim_x = player->facing_x;
        aim_y = player->facing_y;
    }
    const int attack = kicker < SOCCER_TEAM_SIZE ? 1 : -1;
    if (aim_x * attack <= 0) {
        return false;
    }
    target->x = (attack > 0 ? SOCCER_RIGHT + SOCCER_GOAL_LINE_CROSSING_OFFSET :
                              SOCCER_LEFT - SOCCER_GOAL_LINE_CROSSING_OFFSET) *
                SOCCER_ONE;
    int target_y = SOCCER_GOAL_CENTRE_Y;
    if (aim_y < 0) {
        target_y -= SOCCER_GOAL_LANE_OFFSET;
    } else if (aim_y > 0) {
        target_y += SOCCER_GOAL_LANE_OFFSET;
    }
    if (aim_y != 0 && power > SOCCER_SHOT_HIGH_POWER_START) {
        const unsigned int excess  = power - SOCCER_SHOT_HIGH_POWER_START;
        const unsigned int span    = SOCCER_SHOT_MAX_CHARGE - SOCCER_SHOT_HIGH_POWER_START;
        const int error            = (int) ((excess * SOCCER_SHOT_MAX_PLACEMENT_ERROR + span - 1U) / span);
        target_y                  += aim_y < 0 ? -error : error;
    }
    target->y = target_y * SOCCER_ONE;
    return true;
}

static soccer_vec_t through_destination(const soccer_game_t* game, unsigned int receiver)
{
    const int direction           = receiver < SOCCER_TEAM_SIZE ? 1 : -1;
    const soccer_player_t* player = &game->players[receiver];
    return (soccer_vec_t) {clamp(player->position.x + direction * 80 * SOCCER_ONE, (SOCCER_LEFT + 24) * SOCCER_ONE,
                                 (SOCCER_RIGHT - 24) * SOCCER_ONE),
                           clamp(player->position.y + player->facing_y * 32 * SOCCER_ONE,
                                 (SOCCER_TOP + 24) * SOCCER_ONE, (SOCCER_BOTTOM - 24) * SOCCER_ONE)};
}

/* Lead a receiver upfield; the ball remains a normal interceptable ground ball. */
static void through_ball(soccer_game_t* game)
{
    if (game->pass_target < 0) {
        game->velocity.x = game->velocity.x * 3 / 2;
        game->velocity.y = game->velocity.y * 3 / 2;
        return;
    }
    const unsigned int receiver = (unsigned int) game->pass_target;
    game->pass_destination      = through_destination(game, receiver);
    game->through_pass          = true;
    /* Ground drag sums to about 26 frame-lengths over 40 ticks, or 32 over
       60 ticks. Red runners need the longer arrival time at their slower pace. */
    int duration = 26;
    if (receiver >= SOCCER_TEAM_SIZE) {
        duration = 32;
        if (game->difficulty == SOCCER_EASY) {
            duration = 36;
        } else if (game->difficulty == SOCCER_HARD) {
            duration = 30;
        }
    }
    game->velocity.x = (game->pass_destination.x - game->ball.x) / duration;
    game->velocity.y = (game->pass_destination.y - game->ball.y) / duration;
}

static void loft(soccer_game_t* game)
{
    game->ball_height       = 6 * SOCCER_ONE;
    game->vertical_velocity = 5 * SOCCER_ONE;
    if (game->pass_target >= 0) {
        /* Flight lasts about 42 ticks; allow for slight air drag so an assisted
           lob lands near its receiver rather than using ground-pass power. */
        game->velocity.x = (game->pass_destination.x - game->ball.x) / 38;
        game->velocity.y = (game->pass_destination.y - game->ball.y) / 38;
    }
}

static bool keeper_save(soccer_game_t* game)
{
    if (game->keeper_owner >= 0 || game->ball_height > 24 * SOCCER_ONE) {
        return false;
    }
    for (unsigned int i = 0U; i < 2U; ++i) {
        if (game->owner >= 0 && (unsigned int) game->owner / SOCCER_TEAM_SIZE == i) {
            continue;
        }
        soccer_player_t* keeper = &game->keepers[i];
        const bool recovering   = game->dives[i].ticks > 0U && game->dives[i].ticks <= SOCCER_KEEPER_RECOVERY_TICKS;
        if (!recovering && keeper->tackle_cooldown == 0U &&
            distance_squared(keeper->position, game->ball) <= SOCCER_KEEPER_CATCH_RADIUS * SOCCER_KEEPER_CATCH_RADIUS) {
            const int64_t speed_squared =
                (int64_t) game->velocity.x * game->velocity.x + (int64_t) game->velocity.y * game->velocity.y;
            const bool parry = game->owner < 0 && game->velocity.x * keeper->facing_x < 0 &&
                               speed_squared >= (int64_t) 100 * SOCCER_ONE * SOCCER_ONE;
            game->owner           = -1;
            game->last_touch_team = i;
            action(game, SOCCER_EVENT_SAVE);
            if (game->shot_team == (int) (1U - i)) {
                ++game->stats[1U - i].on_target;
                ++game->stats[i].saves;
            }
            game->shot_team = -1;
            if (parry) {
                /* A hard shot spills back into play. Preserve the contact position,
                   and leave a brief opening for either team to reach the rebound. */
                const int side    = game->ball.y < keeper->position.y ? -1 : 1;
                game->velocity.x  = keeper->facing_x * clamp(abs(game->velocity.x) / 2, 3 * SOCCER_ONE, 6 * SOCCER_ONE);
                game->velocity.y  = side * 3 * SOCCER_ONE;
                game->ball_height = 0;
                game->vertical_velocity = 0;
                game->pass_target       = -1;
                game->through_pass      = false;
                game->pass_ticks        = 0U;
                game->last_kicker       = -1;
                game->pickup_delay      = 0U;
                keeper->tackle_cooldown = 18U;
                keeper->kick_ticks      = 16U;
                return true;
            }
            game->keeper_owner      = (int) i;
            game->keeper_ticks      = 45U;
            game->pass_target       = -1;
            game->through_pass      = false;
            game->pass_ticks        = 0U;
            game->velocity          = (soccer_vec_t) {0, 0};
            game->ball              = keeper->position;
            game->ball_height       = 0;
            game->vertical_velocity = 0;
            keeper->kick_ticks      = 16U;
            return true;
        }
    }
    return false;
}

/* Prefer a reachable, unmarked upfield outlet; otherwise clear straight ahead. */
static int keeper_outlet(const soccer_game_t* game, unsigned int team)
{
    int receiver                  = -1;
    int32_t best                  = 300 * 300 + 1;
    const soccer_player_t* keeper = &game->keepers[team];
    for (unsigned int i = team * SOCCER_TEAM_SIZE; i < (team + 1U) * SOCCER_TEAM_SIZE; ++i) {
        const soccer_player_t* player = &game->players[i];
        const int32_t distance        = distance_squared(keeper->position, player->position);
        if ((player->position.x - keeper->position.x) * keeper->facing_x < 64 * SOCCER_ONE || player->jump_ticks > 0U ||
            player->tackle_cooldown > 24U || distance >= best || lane_pressure(game, team, player->position) > 0) {
            continue;
        }
        receiver = (int) i;
        best     = distance;
    }
    return receiver;
}

static void goalkeepers(soccer_game_t* game)
{
    for (unsigned int i = 0U; i < 2U; ++i) {
        soccer_player_t* keeper = &game->keepers[i];
        if (keeper->tackle_cooldown > 0U) {
            --keeper->tackle_cooldown;
        }
        if (keeper->kick_ticks > 0U) {
            --keeper->kick_ticks;
        }
        keeper->moving      = false;
        soccer_dive_t* dive = &game->dives[i];
        if (dive->ticks > 0U) {
            --dive->ticks;
        }
        if (game->keeper_owner == (int) i) {
            game->ball = keeper->position;
            if (--game->keeper_ticks == 0U) {
                const int receiver     = keeper_outlet(game, i);
                game->keeper_owner     = -1;
                game->ball.x          += keeper->facing_x * 20 * SOCCER_ONE;
                game->last_touch_team  = i;
                action(game, SOCCER_EVENT_PASS);
                game->pass_target  = -1;
                game->pass_ticks   = 0U;
                game->through_pass = false;
                game->velocity     = (soccer_vec_t) {keeper->facing_x * SOCCER_SHOT_BASE_SPEED, 0};
                if (receiver >= 0) {
                    game->pass_target      = receiver;
                    game->pass_destination = game->players[receiver].position;
                    game->pass_ticks       = 120U;
                    aim_ball(game, game->pass_destination, SOCCER_SHOT_BASE_SPEED);
                    if (i == 0U) {
                        ++game->passes;
                    }
                }
                keeper->tackle_cooldown = 60U;
                keeper->kick_ticks      = 12U;
                game->last_kicker       = -1;
                game->pickup_delay      = 0U;
            }
            continue;
        }
        /* Observe briefly, sample the intercept once, then commit. A wrong-footed
           keeper cannot home in on a deflection or reverse the chosen direction. */
        const int32_t approach = -game->velocity.x * keeper->facing_x;
        const int32_t distance = (game->ball.x - keeper->position.x) * keeper->facing_x;
        const bool incoming = game->owner < 0 && game->keeper_owner < 0 && approach >= 3 * SOCCER_ONE && distance > 0 &&
                              distance <= SOCCER_KEEPER_DIVE_TRIGGER_DISTANCE * SOCCER_ONE &&
                              game->ball_height <= 24 * SOCCER_ONE;
        if (!incoming) {
            dive->reaction_ticks = 0U;
            dive->observed       = false;
        }
        if (dive->ticks == 0U && dive->reaction_ticks == 0U && !dive->observed && keeper->tackle_cooldown == 0U &&
            incoming && distance / approach <= 18 + SOCCER_KEEPER_REACTION_TICKS) {
            dive->reaction_ticks = SOCCER_KEEPER_REACTION_TICKS;
            dive->observed       = true;
        }
        if (dive->reaction_ticks > 0U && --dive->reaction_ticks == 0U) {
            const int32_t arrival   = distance / approach;
            const int32_t intercept = game->ball.y + game->velocity.y * arrival;
            const int32_t offset    = intercept - keeper->position.y;
            if (arrival <= 18 && intercept >= SOCCER_GOAL_TOP * SOCCER_ONE &&
                intercept <= SOCCER_GOAL_BOTTOM * SOCCER_ONE &&
                abs(offset) > SOCCER_KEEPER_DIVE_THRESHOLD * SOCCER_ONE) {
                dive->ticks     = SOCCER_KEEPER_DIVE_TICKS + SOCCER_KEEPER_RECOVERY_TICKS;
                dive->direction = offset < 0 ? -1 : 1;
            }
        }
        if (dive->ticks > 0U) {
            if (dive->ticks > SOCCER_KEEPER_RECOVERY_TICKS) {
                keeper->position.y = clamp(keeper->position.y + dive->direction * SOCCER_KEEPER_DIVE_SPEED * SOCCER_ONE,
                                           SOCCER_GOAL_TOP * SOCCER_ONE, SOCCER_GOAL_BOTTOM * SOCCER_ONE);
                keeper->moving     = true;
            }
            continue;
        }
        int32_t target_y = SOCCER_PITCH_CENTRE_Y * SOCCER_ONE;
        if (abs(game->ball.x - keeper->position.x) < SOCCER_KEEPER_TRACK_DISTANCE * SOCCER_ONE) {
            const int lead = dive->reaction_ticks > 0U ? 0 : 6;
            target_y       = clamp(game->ball.y + game->velocity.y * lead, SOCCER_KEEPER_STANDING_TOP * SOCCER_ONE,
                                   SOCCER_KEEPER_STANDING_BOTTOM * SOCCER_ONE);
        }
        const int32_t motion  = clamp(target_y - keeper->position.y, -320, 320);
        keeper->position.y   += motion;
        keeper->moving        = motion != 0;
        if (keeper->moving) {
            ++keeper->animation;
        }
    }
}

static void restart(soccer_game_t* game, soccer_restart_t kind, unsigned int team, soccer_vec_t spot)
{
    memset(game->dives, 0, sizeof(game->dives));
    game->shot_charge   = 0U;
    game->shot_team     = -1;
    const int direction = team == 0U ? 1 : -1;
    spot.x              = clamp(spot.x, (SOCCER_LEFT + 8) * SOCCER_ONE, (SOCCER_RIGHT - 8) * SOCCER_ONE);
    spot.y              = clamp(spot.y, (SOCCER_TOP + 8) * SOCCER_ONE, (SOCCER_BOTTOM - 8) * SOCCER_ONE);
    if (kind == SOCCER_GOAL_KICK) {
        spot = game->keepers[team].position;
    }
    game->ball              = spot;
    game->velocity          = (soccer_vec_t) {0, 0};
    game->owner             = -1;
    game->keeper_owner      = -1;
    game->keeper_ticks      = 0U;
    game->pass_target       = -1;
    game->through_pass      = false;
    game->pass_ticks        = 0U;
    game->last_kicker       = -1;
    game->pickup_delay      = 0U;
    game->ball_height       = 0;
    game->vertical_velocity = 0;
    game->restart_kind      = kind;
    game->restart_team      = team;
    game->restart_ticks     = 180U;
    game->restart_taker     = nearest_player(game, team, -1);
    game->restart_receiver  = nearest_player(game, team, kind == SOCCER_GOAL_KICK ? -1 : (int) game->restart_taker);
    game->mode              = SOCCER_RESTART;
    game->feedback_ticks    = 0U;
    action(game, SOCCER_EVENT_WHISTLE);
    if (team == 0U) {
        game->controlled = game->restart_taker;
    }
    soccer_player_t* taker  = &game->players[game->restart_taker];
    soccer_vec_t outlet     = spot;
    outlet.x               += direction * 110 * SOCCER_ONE;
    outlet.y               += (spot.y < 480 * SOCCER_ONE ? 100 : -100) * SOCCER_ONE;
    if (kind == SOCCER_CORNER) {
        outlet.x = spot.x + (team == 0U ? -110 : 110) * SOCCER_ONE;
    }
    outlet.x = clamp(outlet.x, (SOCCER_LEFT + 40) * SOCCER_ONE, (SOCCER_RIGHT - 40) * SOCCER_ONE);
    outlet.y = clamp(outlet.y, (SOCCER_TOP + 40) * SOCCER_ONE, (SOCCER_BOTTOM - 40) * SOCCER_ONE);
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        game->players[i].moving          = false;
        game->players[i].sprinting       = false;
        game->players[i].kick_ticks      = 0U;
        game->players[i].slide_ticks     = 0U;
        game->players[i].jump_ticks      = 0U;
        game->players[i].jump_cooldown   = 0U;
        game->players[i].headed          = false;
        game->players[i].tackle_cooldown = 0U;
        if (distance_squared(game->players[i].position, spot) < 80 * 80) {
            /* Arcade placement provides room for the restart instead of allowing
               an opponent standing on the touchline to take it immediately. */
            game->players[i].position   = outlet;
            game->players[i].position.y = clamp(outlet.y + (i % 2U == 0U ? -50 : 50) * SOCCER_ONE,
                                                (SOCCER_TOP + 16) * SOCCER_ONE, (SOCCER_BOTTOM - 16) * SOCCER_ONE);
        }
    }
    game->players[game->restart_receiver].position = outlet;
    if (kind != SOCCER_GOAL_KICK) {
        taker->position = spot;
        taker->facing_x = outlet.x > spot.x ? 1 : -1;
        taker->facing_y = outlet.y > spot.y ? 1 : -1;
    }
}

static void restart_step(soccer_game_t* game, soccer_input_t input)
{
    --game->restart_ticks;
    soccer_player_t* taker = &game->players[game->restart_taker];
    if (game->restart_team == 0U && (input.dx != 0 || input.dy != 0)) {
        taker->facing_x = clamp(input.dx, -1, 1);
        taker->facing_y = clamp(input.dy, -1, 1);
    }
    const bool automatic = game->restart_team == 1U || game->restart_kind == SOCCER_GOAL_KICK;
    if (game->restart_ticks > 90U ||
        (!automatic && game->restart_ticks > 0U && !input.pass && !input.shoot && !input.lob)) {
        return;
    }
    game->mode            = SOCCER_PLAY;
    game->last_touch_team = game->restart_team;
    game->owner           = -1;
    game->pickup_delay    = 24U;
    game->last_kicker     = (int) game->restart_taker;
    if (game->restart_kind == SOCCER_GOAL_KICK) {
        soccer_player_t* keeper  = &game->keepers[game->restart_team];
        game->ball.x            += keeper->facing_x * 20 * SOCCER_ONE;
        keeper->kick_ticks       = 12U;
        keeper->tackle_cooldown  = 60U;
        game->last_kicker        = -1;
    } else {
        taker->kick_ticks = 12U;
    }
    if (game->restart_team == 0U && input.shoot && game->restart_kind == SOCCER_CORNER) {
        const int speed =
            taker->facing_x != 0 && taker->facing_y != 0 ? SOCCER_SHOT_DIAGONAL_SPEED : SOCCER_SHOT_BASE_SPEED;
        game->velocity = (soccer_vec_t) {taker->facing_x * speed, taker->facing_y * speed};
        ++game->shots;
        ++game->stats[0].shots;
        game->shot_team = 0;
        action(game, SOCCER_EVENT_SHOT);
    } else {
        if (game->restart_team == 0U) {
            ++game->passes;
        }
        const soccer_vec_t destination = game->players[game->restart_receiver].position;
        aim_ball(game, destination, 1920);
        game->pass_target      = (int) game->restart_receiver;
        game->pass_destination = destination;
        game->pass_ticks       = 120U;
        action(game, SOCCER_EVENT_PASS);
        if (input.lob && game->restart_team == 0U && game->restart_kind == SOCCER_CORNER) {
            loft(game);
        }
    }
}

/* Whole-ball boundary crossings are ordered by their fraction of the movement,
   so a diagonal ball leaving near a corner awards the first line it crossed. */
static bool boundary(soccer_game_t* game, soccer_vec_t previous, int32_t previous_height)
{
    const int32_t limits[] = {
        (SOCCER_LEFT - SOCCER_GOAL_LINE_CROSSING_OFFSET) * SOCCER_ONE,
        (SOCCER_RIGHT + SOCCER_GOAL_LINE_CROSSING_OFFSET) * SOCCER_ONE,
        (SOCCER_TOP - SOCCER_BALL_RADIUS) * SOCCER_ONE,
        (SOCCER_BOTTOM + SOCCER_BALL_RADIUS) * SOCCER_ONE,
    };
    int edge         = -1;
    int32_t earliest = 65537;
    for (unsigned int i = 0U; i < 4U; ++i) {
        const int32_t from = i < 2U ? previous.x : previous.y;
        const int32_t to   = i < 2U ? game->ball.x : game->ball.y;
        const bool crossed = i % 2U == 0U ? from > limits[i] && to <= limits[i] : from < limits[i] && to >= limits[i];
        if (crossed) {
            const int32_t fraction = (int32_t) ((int64_t) (limits[i] - from) * 65536 / (to - from));
            if (fraction < earliest) {
                earliest = fraction;
                edge     = (int) i;
            }
        }
    }
    if (edge < 0) {
        return false;
    }
    const soccer_vec_t spot = {previous.x + (int32_t) ((int64_t) (game->ball.x - previous.x) * earliest / 65536),
                               previous.y + (int32_t) ((int64_t) (game->ball.y - previous.y) * earliest / 65536)};
    const int32_t crossing_height =
        previous_height + (int32_t) ((int64_t) (game->ball_height - previous_height) * earliest / 65536);
    if (edge < 2) {
        if (crossing_height <= (SOCCER_GOAL_HEIGHT - SOCCER_BALL_RADIUS) * SOCCER_ONE &&
            spot.y >= SOCCER_GOAL_SCORE_TOP * SOCCER_ONE && spot.y <= SOCCER_GOAL_SCORE_BOTTOM * SOCCER_ONE) {
            if (edge == 1) {
                ++game->goals;
            } else {
                ++game->opponent_goals;
            }
            if (game->shot_team == 1 - edge) {
                ++game->stats[1 - edge].on_target;
            }
            game->shot_team = -1;
            action(game, SOCCER_EVENT_GOAL);
            game->owner        = -1;
            game->kickoff_team = (unsigned int) edge;
            game->mode         = SOCCER_GOAL;
            game->goal_ticks   = 90U;
            game->velocity     = (soccer_vec_t) {0, 0};
        } else if (game->last_touch_team == (unsigned int) edge) {
            const soccer_vec_t corner = {edge == 0 ? SOCCER_LEFT * SOCCER_ONE : SOCCER_RIGHT * SOCCER_ONE,
                                         (spot.y < 480 * SOCCER_ONE ? SOCCER_TOP : SOCCER_BOTTOM) * SOCCER_ONE};
            restart(game, SOCCER_CORNER, 1U - (unsigned int) edge, corner);
        } else {
            restart(game, SOCCER_GOAL_KICK, (unsigned int) edge, spot);
        }
    } else {
        restart(game, SOCCER_THROW_IN, 1U - game->last_touch_team, spot);
    }
    return true;
}

static void ball_step(soccer_game_t* game)
{
    /* Four bounded substeps keep a maximum-speed shot from crossing a post unseen. */
    for (unsigned int step = 0U; step < 4U; ++step) {
        const int32_t previous_height = game->ball_height;
        if (game->ball_height > 0 || game->vertical_velocity != 0) {
            game->vertical_velocity -= 16;
            game->ball_height       += game->vertical_velocity / 4;
            if (game->ball_height <= 0) {
                game->ball_height       = 0;
                game->vertical_velocity = -game->vertical_velocity * 2 / 5;
                if (game->vertical_velocity < 256) {
                    game->vertical_velocity = 0;
                }
            }
        }
        const soccer_vec_t previous  = game->ball;
        game->ball.x                += game->velocity.x / 4;
        game->ball.y                += game->velocity.y / 4;
        if (keeper_save(game)) {
            return;
        }
        const int posts[] = {SOCCER_GOAL_TOP, SOCCER_GOAL_BOTTOM};
        for (unsigned int end = 0U; end < 2U; ++end) {
            const int goal_x = end == 0U ? SOCCER_LEFT : SOCCER_RIGHT;
            for (unsigned int post = 0U; post < 2U; ++post) {
                const int32_t dx = (game->ball.x - goal_x * SOCCER_ONE) / SOCCER_ONE;
                const int32_t dy = (game->ball.y - posts[post] * SOCCER_ONE) / SOCCER_ONE;
                if (game->ball_height < (SOCCER_GOAL_HEIGHT + SOCCER_BALL_RADIUS) * SOCCER_ONE &&
                    dx * dx + dy * dy < SOCCER_GOAL_COLLISION_RADIUS * SOCCER_GOAL_COLLISION_RADIUS) {
                    action(game, SOCCER_EVENT_POST);
                    game->ball       = previous;
                    game->velocity.x = -game->velocity.x * 3 / 4;
                    game->velocity.y = -game->velocity.y * 3 / 4;
                }
            }
        }
        for (unsigned int end = 0U; end < 2U; ++end) {
            const int goal_x = end == 0U ? SOCCER_LEFT : SOCCER_RIGHT;
            if (game->velocity.x * (end == 0U ? -1 : 1) > 0 &&
                abs(game->ball.x - goal_x * SOCCER_ONE) < SOCCER_GOAL_COLLISION_RADIUS * SOCCER_ONE &&
                game->ball.y > SOCCER_GOAL_TOP * SOCCER_ONE && game->ball.y < SOCCER_GOAL_BOTTOM * SOCCER_ONE &&
                abs(game->ball_height - SOCCER_GOAL_HEIGHT * SOCCER_ONE) < (SOCCER_BALL_RADIUS + 2) * SOCCER_ONE) {
                game->ball              = previous;
                game->velocity.x        = -game->velocity.x * 3 / 4;
                game->vertical_velocity = -abs(game->vertical_velocity) / 2;
                action(game, SOCCER_EVENT_POST);
            }
        }
        if (boundary(game, previous, previous_height)) {
            return;
        }
    }
    const int friction = game->ball_height > 0 ? 255 : 250;
    game->velocity.x   = game->velocity.x * friction / 256;
    game->velocity.y   = game->velocity.y * friction / 256;
    if (game->velocity.x > -8 && game->velocity.x < 8) {
        game->velocity.x = 0;
    }
    if (game->velocity.y > -8 && game->velocity.y < 8) {
        game->velocity.y = 0;
    }
}

static bool opponent_shooting_position(const soccer_game_t* game)
{
    return game->owner >= SOCCER_TEAM_SIZE && game->ball.x < (SOCCER_LEFT + 220) * SOCCER_ONE &&
           abs(game->ball.y - SOCCER_PITCH_CENTRE_Y * SOCCER_ONE) < 45 * SOCCER_ONE;
}

static bool opponent_shot_target(const soccer_game_t* game, soccer_vec_t* target)
{
    if (!opponent_shooting_position(game)) {
        return false;
    }
    target->x = SOCCER_LEFT * SOCCER_ONE;
    target->y =
        (SOCCER_GOAL_CENTRE_Y +
         (game->ball.y < SOCCER_PITCH_CENTRE_Y * SOCCER_ONE ? SOCCER_GOAL_LANE_OFFSET : -SOCCER_GOAL_LANE_OFFSET)) *
        SOCCER_ONE;
    if (lane_pressure(game, 1U, *target) == 0) {
        return true;
    }
    target->y = 2 * SOCCER_GOAL_CENTRE_Y * SOCCER_ONE - target->y;
    return lane_pressure(game, 1U, *target) == 0;
}

void soccer_game_step(soccer_game_t* game, soccer_input_t input)
{
    if (game->paused || game->mode == SOCCER_TITLE || game->mode == SOCCER_FULL_TIME) {
        return;
    }
    if (game->mode != SOCCER_PLAY) {
        game->support_ticks[0] = 0U;
        game->support_ticks[1] = 0U;
    }
    game->event = SOCCER_EVENT_NONE;
    if (game->feedback_ticks > 0U) {
        --game->feedback_ticks;
    }
    if (game->mode == SOCCER_RESTART) {
        restart_step(game, input);
        return;
    }
    if (game->mode == SOCCER_GOAL) {
        if (--game->goal_ticks == 0U) {
            if (game->match_ticks == 0U) {
                game->mode = SOCCER_FULL_TIME;
                action(game, SOCCER_EVENT_FINISH);
            } else {
                kickoff(game, game->kickoff_team);
            }
        }
        return;
    }
    if (game->mode == SOCCER_KICKOFF) {
        /* Aim during setup without moving before the whistle. This also keeps
           a briefly tapped direction for the first live pass. */
        if (game->owner == (int) game->controlled && (input.dx != 0 || input.dy != 0)) {
            game->players[game->controlled].facing_x = clamp(input.dx, -1, 1);
            game->players[game->controlled].facing_y = clamp(input.dy, -1, 1);
        }
        if (--game->goal_ticks == 0U) {
            game->mode = SOCCER_PLAY;
            action(game, SOCCER_EVENT_WHISTLE);
        }
        return;
    }
    if (game->match_ticks == 0U) {
        game->mode = SOCCER_FULL_TIME;
        action(game, SOCCER_EVENT_FINISH);
        return;
    }
    --game->match_ticks;
    if (game->keeper_owner >= 0) {
        ++game->stats[game->keeper_owner].possession;
    } else if (game->owner >= 0) {
        ++game->stats[(unsigned int) game->owner / SOCCER_TEAM_SIZE].possession;
    }
    if (input.switch_player) {
        game->shot_charge = 0U;
        if (game->owner >= 0 && game->owner < SOCCER_TEAM_SIZE) {
            game->controlled = (game->controlled + 1U) % SOCCER_TEAM_SIZE;
        } else {
            game->controlled = defensive_selection(game);
        }
    }
    input.dx = clamp(input.dx, -1, 1);
    input.dy = clamp(input.dy, -1, 1);
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        soccer_player_t* player = &game->players[i];
        if (player->stamina < 90U &&
            (i != game->controlled || !input.sprint || (input.dx == 0 && input.dy == 0) || player->sprint_rest > 0U)) {
            ++player->stamina;
        }
        if (player->sprint_rest > 0U) {
            --player->sprint_rest;
        }
        player->sprinting = false;
        if (game->players[i].jump_ticks > 0U) {
            --game->players[i].jump_ticks;
        }
        if (game->players[i].jump_cooldown > 0U) {
            --game->players[i].jump_cooldown;
        }
        if (game->players[i].tackle_cooldown > 0U) {
            --game->players[i].tackle_cooldown;
        }
        if (game->players[i].kick_ticks > 0U) {
            --game->players[i].kick_ticks;
        }
    }
    if (game->possession_ticks > 0U) {
        --game->possession_ticks;
    }
    if (game->pickup_delay > 0U) {
        --game->pickup_delay;
    }
    if (game->pass_ticks > 0U && --game->pass_ticks == 0U) {
        game->pass_target  = -1;
        game->through_pass = false;
    }
    /* Remember movement even when this tick finishes the slide. Normal
       steering must wait until the next tick, including after a control handoff. */
    bool slid[SOCCER_PLAYER_COUNT];
    for (unsigned int i = 0U; i < SOCCER_PLAYER_COUNT; ++i) {
        slid[i] = game->players[i].slide_ticks > 0U;
        slide_step(game, i);
    }
    if (slid[game->controlled]) {
        input.dx = input.dy = 0;
        input.shoot = input.shoot_held = input.pass = input.lob = input.through = false;
        game->shot_charge = game->pass_charge = 0U;
    }
    soccer_player_t* controlled = &game->players[game->controlled];
    controlled->sprinting       = input.sprint && (input.dx != 0 || input.dy != 0) && controlled->stamina > 0U &&
                            controlled->sprint_rest == 0U && controlled->jump_ticks == 0U &&
                            controlled->tackle_cooldown == 0U;
    const soccer_vec_t before_move = controlled->position;
    if (!slid[game->controlled]) {
        move_player(controlled, input.dx, input.dy);
    }
    if (controlled->sprinting && before_move.x == controlled->position.x && before_move.y == controlled->position.y) {
        controlled->sprinting = false;
    }
    if (controlled->sprinting && --controlled->stamina == 0U) {
        controlled->sprint_rest = 60U;
    }
    /* Resolve the human attempt first so a simultaneous AI attempt cannot steal
       the ball before the player's press is considered. */
    const bool tackling = input.shoot && game->owner >= 0 &&
                          (unsigned int) game->owner / SOCCER_TEAM_SIZE != game->controlled / SOCCER_TEAM_SIZE;
    if (tackling) {
        tackle(game, game->controlled);
    }
    if (input.shoot && game->owner < 0 && game->keeper_owner < 0 && game->ball_height > 4 * SOCCER_ONE) {
        jump(&game->players[game->controlled]);
    }
    teams(game, slid);
    separate_off_ball_players(game);
    headers(game);
    goalkeepers(game);
    collect(game);
    if (game->owner != (int) game->controlled || game->charge_player != game->controlled || input.pass || input.lob ||
        input.through) {
        game->shot_charge = 0U;
    }
    if (game->owner >= 0) {
        const soccer_player_t* player    = &game->players[game->owner];
        const int offset                 = player->facing_x != 0 && player->facing_y != 0 ? 2534 : 3584;
        const soccer_vec_t previous_ball = game->ball;
        game->last_touch_team            = (unsigned int) game->owner / SOCCER_TEAM_SIZE;
        game->ball.x                     = player->position.x + player->facing_x * offset;
        game->ball.y                     = player->position.y + player->facing_y * offset;
        if (keeper_save(game) || boundary(game, previous_ball, 0)) {
            game->shot_charge = 0U;
            if (game->match_ticks == 0U && game->mode != SOCCER_GOAL) {
                game->mode = SOCCER_FULL_TIME;
                action(game, SOCCER_EVENT_FINISH);
            }
            return;
        }
        game->velocity    = (soccer_vec_t) {0, 0};
        bool release_shot = input.shoot && !input.shoot_held;
        if (game->owner == (int) game->controlled && !tackling && !input.pass && !input.lob && !input.through) {
            if (input.shoot && input.shoot_held && game->shot_charge == 0U) {
                game->charge_player = game->controlled;
                game->shot_charge   = 1U;
            } else if (game->shot_charge > 0U && input.shoot_held) {
                ++game->shot_charge;
            }
            if (game->shot_charge > 0U && (!input.shoot_held || game->shot_charge >= SOCCER_SHOT_MAX_CHARGE)) {
                release_shot = true;
            }
        }
        soccer_vec_t shot_target       = {0, 0};
        const bool clear_opponent_shot = player->slide_ticks == 0U && opponent_shot_target(game, &shot_target);
        if (release_shot && !tackling && game->owner == (int) game->controlled) {
            const unsigned int kicker = game->controlled;
            const unsigned int power  = game->shot_charge;
            soccer_vec_t target;
            const bool aimed  = human_shot_target(game, kicker, input, power, &target);
            game->shot_charge = 0U;
            kick(game, false);
            const int speed = SOCCER_SHOT_BASE_SPEED * (SOCCER_SHOT_POWER_BASE + (int) power) / SOCCER_SHOT_POWER_BASE;
            if (aimed) {
                aim_ball(game, target, speed);
            } else {
                game->velocity.x =
                    game->velocity.x * (SOCCER_SHOT_POWER_BASE + (int32_t) power) / SOCCER_SHOT_POWER_BASE;
                game->velocity.y =
                    game->velocity.y * (SOCCER_SHOT_POWER_BASE + (int32_t) power) / SOCCER_SHOT_POWER_BASE;
            }
        } else if ((input.pass || input.lob || input.through) && game->owner < SOCCER_TEAM_SIZE) {
            kick(game, true);
            if (input.lob) {
                loft(game);
            } else if (input.through) {
                through_ball(game);
            }
        } else if (clear_opponent_shot) {
            kick(game, false);
            aim_ball(game, shot_target, SOCCER_SHOT_BASE_SPEED);
        } else if (game->owner >= SOCCER_TEAM_SIZE && game->possession_ticks == 0U && player->slide_ticks == 0U) {
            const unsigned int defender = nearest_player(game, 0U, -1);
            const int receiver          = pass_receiver(game, (unsigned int) game->owner);
            if (receiver >= 0) {
                const soccer_vec_t destination = through_destination(game, (unsigned int) receiver);
                const bool through             = (unsigned int) receiver % SOCCER_TEAM_SIZE < SOCCER_DEFENDER_FIRST &&
                                     game->players[receiver].position.x < game->ball.x - 40 * SOCCER_ONE &&
                                     destination.x > (SOCCER_LEFT + 100) * SOCCER_ONE &&
                                     lane_pressure(game, 1U, destination) == 0;
                const bool blocked_shot_outlet =
                    opponent_shooting_position(game) && lane_pressure(game, 1U, game->players[receiver].position) == 0;
                if (through || blocked_shot_outlet ||
                    distance_squared(game->players[defender].position, game->ball) < 90 * 90) {
                    kick(game, true);
                    if (through) {
                        through_ball(game);
                    }
                }
            }
        }
    }
    if (game->owner < 0 && game->keeper_owner < 0) {
        ball_step(game);
        if (game->mode == SOCCER_PLAY) {
            collect(game);
        }
    }
    if (game->match_ticks == 0U && game->mode != SOCCER_GOAL) {
        game->mode = SOCCER_FULL_TIME;
        action(game, SOCCER_EVENT_FINISH);
    }
}

/* Require a sustained, useful improvement before handing off defensive control. */
static void automatic_selection(soccer_game_t* game, soccer_input_t buttons)
{
    if (game->paused) {
        return;
    }
    if (game->selection_cooldown > 0U) {
        --game->selection_cooldown;
    }
    if (buttons.shoot) {
        game->selection_cooldown = 45U;
    }
    if (game->players[game->controlled].slide_ticks > 0U) {
        game->selection_ticks = 0U;
        return;
    }
    if (game->mode != SOCCER_PLAY || (game->owner >= 0 && game->owner < SOCCER_TEAM_SIZE) || game->keeper_owner >= 0 ||
        (game->pass_target >= 0 && game->pass_target < SOCCER_TEAM_SIZE) || game->selection_cooldown > 0U ||
        game->players[game->controlled].jump_ticks > 0U) {
        game->selection_ticks = 0U;
        return;
    }
    const unsigned int candidate     = defensive_selection(game);
    const soccer_player_t* player    = &game->players[candidate];
    const int32_t current_distance   = distance_squared(game->players[game->controlled].position, game->ball);
    const int32_t candidate_distance = distance_squared(player->position, game->ball);
    if (player->jump_ticks > 0U || player->tackle_cooldown > 24U || current_distance < 90 * 90 ||
        candidate_distance * 2 + 40 * 40 >= current_distance) {
        game->selection_ticks = 0U;
        return;
    }
    if (game->selection_candidate != candidate) {
        game->selection_candidate = candidate;
        game->selection_ticks     = 0U;
    }
    if (++game->selection_ticks >= 12U) {
        game->controlled         = candidate;
        game->selection_ticks    = 0U;
        game->selection_cooldown = 60U;
        game->pass_charge        = 0U;
        game->shot_charge        = 0U;
    }
}

/* Translate the two physical action buttons into rules actions. Keeping the
   gesture on simulation ticks makes tap/hold identical on host and Tab5. */
void soccer_game_arcade_step(soccer_game_t* game, soccer_input_t buttons)
{
    soccer_input_t input = {
        .dx = buttons.dx, .dy = buttons.dy, .shoot = buttons.shoot, .shoot_held = buttons.shoot_held};
    const bool restart = game->mode == SOCCER_RESTART && game->restart_team == 0U &&
                         game->restart_kind != SOCCER_GOAL_KICK && game->restart_ticks <= 90U;
    const bool possession = game->mode == SOCCER_PLAY && game->owner == (int) game->controlled;
    if (game->paused || (!possession && !restart) || game->pass_player != game->controlled || buttons.shoot) {
        game->pass_charge = 0U;
    }
    if (!game->paused && buttons.pass && !buttons.shoot) {
        game->shot_charge = 0U;
        if (possession || restart) {
            game->pass_player = game->controlled;
            game->pass_charge = 1U;
        }
    }
    if (game->pass_charge > 0U) {
        if (!buttons.pass_held) {
            input.pass        = true;
            game->pass_charge = 0U;
        } else if (++game->pass_charge >= 18U) {
            input.lob         = true;
            game->pass_charge = 0U;
        }
    }
    const soccer_mode_t mode               = game->mode;
    const unsigned int previous_controlled = game->controlled;
    soccer_game_step(game, input);
    if (game->controlled != previous_controlled) {
        game->selection_cooldown = 60U;
    }
    automatic_selection(game, buttons);
    if (game->mode != mode || (game->mode == SOCCER_PLAY && game->owner != (int) game->pass_player)) {
        game->pass_charge = 0U;
    }
}
