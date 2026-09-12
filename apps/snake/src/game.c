#include <snake/game.h>

#include <string.h>

uint32_t snake_game_score(const snake_game_t* game)
{
    return ((uint32_t) game->length - 4U) * 10U;
}

uint32_t snake_game_interval(const snake_game_t* game)
{
    const uint32_t levels = snake_game_score(game) / 50U;
    return levels >= 10U ? 60U : 160U - levels * 10U;
}

static void place_food(snake_game_t* game)
{
    bool occupied[SNAKE_CAPACITY] = {false};
    for (unsigned int i = 0U; i < game->length; ++i) {
        occupied[game->cells[i]] = true;
    }
    game->random        ^= game->random << 13U;
    game->random        ^= game->random >> 17U;
    game->random        ^= game->random << 5U;
    uint32_t free_index  = game->random % (SNAKE_CAPACITY - (uint32_t) game->length);
    for (uint16_t cell = 0U; cell < SNAKE_CAPACITY; ++cell) {
        if (!occupied[cell]) {
            if (free_index == 0U) {
                game->food = cell;
                return;
            }
            --free_index;
        }
    }
}

void snake_game_start(snake_game_t* game, uint64_t now)
{
    game->length = 4U;
    for (uint16_t i = 0U; i < game->length; ++i) {
        game->cells[i] = (uint16_t) (9U * SNAKE_COLUMNS + 10U - i);
    }
    game->direction  = SNAKE_RIGHT;
    game->turn_count = 0U;
    game->mode       = SNAKE_PLAYING;
    game->deadline   = now + snake_game_interval(game);
    place_food(game);
}

void snake_game_init(snake_game_t* game, uint32_t seed)
{
    memset(game, 0, sizeof(*game));
    game->random = seed == 0U ? 1U : seed;
    snake_game_start(game, 0U);
    game->mode = SNAKE_TITLE;
}

bool snake_game_turn(snake_game_t* game, snake_direction_t direction)
{
    if (game->mode != SNAKE_PLAYING || game->turn_count == 2U || direction < SNAKE_UP || direction > SNAKE_LEFT) {
        return false;
    }
    const snake_direction_t previous = game->turn_count == 0U ? game->direction : game->turns[game->turn_count - 1U];
    if (direction == previous || ((unsigned int) direction + 2U) % 4U == (unsigned int) previous) {
        return false;
    }
    game->turns[game->turn_count++] = direction;
    return true;
}

void snake_game_pause(snake_game_t* game, uint64_t now)
{
    if (game->mode == SNAKE_PLAYING) {
        game->mode       = SNAKE_PAUSED;
        game->turn_count = 0U;
    } else if (game->mode == SNAKE_PAUSED) {
        game->mode     = SNAKE_PLAYING;
        game->deadline = now + snake_game_interval(game);
    }
}

bool snake_game_update(snake_game_t* game, uint64_t now)
{
    if (game->mode != SNAKE_PLAYING || now < game->deadline) {
        return false;
    }
    if (game->turn_count != 0U) {
        game->direction = game->turns[0];
        game->turns[0]  = game->turns[1];
        --game->turn_count;
    }
    int x = game->cells[0] % SNAKE_COLUMNS;
    int y = game->cells[0] / SNAKE_COLUMNS;
    switch (game->direction) {
        case SNAKE_UP: --y; break;
        case SNAKE_RIGHT: ++x; break;
        case SNAKE_DOWN: ++y; break;
        case SNAKE_LEFT: --x; break;
    }
    if (x < 0 || x >= SNAKE_COLUMNS || y < 0 || y >= SNAKE_ROWS) {
        game->mode = SNAKE_OVER;
        return true;
    }
    const uint16_t next         = (uint16_t) (y * SNAKE_COLUMNS + x);
    const bool growing          = next == game->food;
    const unsigned int retained = growing ? game->length : (unsigned int) game->length - 1U;
    for (unsigned int i = 0U; i < retained; ++i) {
        if (game->cells[i] == next) {
            game->mode = SNAKE_OVER;
            return true;
        }
    }
    memmove(&game->cells[1], game->cells, retained * sizeof(game->cells[0]));
    game->cells[0] = next;
    if (growing) {
        ++game->length;
        if (snake_game_score(game) > game->best) {
            game->best = snake_game_score(game);
        }
        if (game->length == SNAKE_CAPACITY) {
            game->mode = SNAKE_WON;
        } else {
            place_food(game);
        }
    }
    /* A delayed frame never causes a burst of invisible catch-up moves. */
    game->deadline = now + snake_game_interval(game);
    return true;
}
