#include <pool/render.h>
#include <pool/input.h>
#include <pool/clock.h>
#include <pool/ai.h>
#include <pool/sound.h>
#include <string.h>
#include <tabos/runtime_time.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/input.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <unistd.h>

static bool active(const pool_game_t* game, const pool_input_t* controls)
{
    return !game->title && !game->help && !game->paused && !game->restart_pending && !game->rules.complete &&
           (game->shot_set || pool_input_active(controls, game) || pool_game_computer_turn(game) ||
            pool_game_animating(game));
}

static bool sound_enabled(const pool_game_t* game)
{
    return !game->muted && !game->title && !game->help && !game->paused && !game->restart_pending;
}

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--two-player") != 0)) {
        fprintf(stderr, "usage: pool [--two-player]\n");
        return 1;
    }
    uint32_t tty = 0U;
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty) != 0 ||
        ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, tty | (uint32_t) TABOS_TTY_MODE_RAW_INPUT) != 0) {
        fprintf(stderr, "pool: raw keyboard unavailable\n");
        return 1;
    }
    tabos_graphics_t graphics = {.width = POOL_CANVAS_WIDTH, .height = POOL_CANVAS_HEIGHT};
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "pool: graphics unavailable\n");
        return 1;
    }
    tabos_wait_item_t input = {.source = tabos_input_wait_source(), .events = TABOS_WAIT_READABLE};
    int status              = input.source == TABOS_WAIT_SOURCE_INVALID ? 1 : 0;
    pool_game_t game;
    pool_game_reset(&game);
    game.computer      = argc == 1;
    game.title         = true;
    pool_sound_t sound = {.stream = TABOS_AUDIO_STREAM_INVALID};
    pool_ai_t ai;
    pool_ai_reset(&ai, UINT32_C(0x504f4f4c));
    pool_input_t controls = {0};
    bool dirty            = true;
    uint64_t previous     = tabos_monotonic_ms();
    pool_clock_t clock    = {0};
    while (!controls.quit && status == 0) {
        pool_sound_effect_t effect = POOL_SOUND_NONE;
        const bool was_active      = active(&game, &controls);
        bool transition            = false;
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            const pool_game_t before = game;
            const bool had_shot      = game.shot_set;
            const bool had_placement = game.placement;
            const bool had_restart   = game.restart_pending;
            const bool had_result    = game.rules.complete;
            const bool changed       = pool_input_event(&controls, &game, &event);
            if (changed &&
                (event.key == TABOS_KEY_P || event.key == TABOS_KEY_R || had_shot != game.shot_set ||
                 had_placement != game.placement || had_restart != game.restart_pending ||
                 had_result != game.rules.complete || before.title != game.title || before.help != game.help)) {
                transition = true;
            }
            if (changed && event.key == TABOS_KEY_ENTER && (had_restart || had_result || before.title)) {
                pool_ai_reset(&ai, UINT32_C(0x504f4f4c));
            }
            const pool_sound_effect_t next = pool_sound_event(&before, &game);
            if (next > effect) {
                effect = next;
            }
            if (changed && (event.key == TABOS_KEY_M || event.key == TABOS_KEY_R || event.key == TABOS_KEY_P ||
                            before.help != game.help || before.title != game.title || had_restart || had_result)) {
                pool_sound_close(&sound);
                if (sound_enabled(&game)) {
                    pool_sound_prepare(&sound);
                }
                effect = POOL_SOUND_NONE;
            }
            dirty = changed || dirty;
            if (controls.quit) {
                break;
            }
        }
        if (controls.quit) {
            break;
        }
        const uint64_t now     = tabos_monotonic_ms();
        const uint64_t elapsed = now - previous;
        previous               = now;
        const bool is_active   = active(&game, &controls);
        if (is_active && was_active && !transition) {
            const unsigned int ticks = pool_clock_advance(&clock, elapsed);
            for (unsigned int i = 0U; i < ticks; ++i) {
                const pool_game_t before = game;
                dirty                    = pool_game_effects_step(&game) || dirty;
                dirty                    = pool_input_step(&controls, &game) || dirty;
                if (game.shot_set) {
                    pool_ai_reset(&ai, ai.seed);
                    dirty = pool_game_step(&game) || dirty;
                } else if (pool_game_computer_turn(&game)) {
                    controls.blocked = controls.held;
                    dirty            = pool_ai_step(&ai, &game) || dirty;
                }
                const pool_sound_effect_t next = pool_sound_event(&before, &game);
                if (next > effect) {
                    effect = next;
                }
            }
        } else {
            /* Never apply idle/pre-transition time to new controls or motion. */
            clock.accumulator = 0U;
        }
        if (sound_enabled(&game)) {
            pool_sound_play(&sound, effect);
        }
        if (dirty && pool_render(&graphics, &game) != 0) {
            status = 1;
            break;
        }
        dirty            = false;
        uint32_t timeout = TABOS_WAIT_TIMEOUT_INFINITE;
        if (active(&game, &controls)) {
            timeout = pool_clock_wait(&clock, tabos_monotonic_ms() - previous);
        }
        if (tabos_wait(&input, 1U, timeout) < 0) {
            status = 1;
        }
    }
    pool_sound_close(&sound);
    if (sound.error != 0) {
        fprintf(stderr, "pool: audio unavailable: %s\n", strerror(sound.error));
    }
    if (tabos_graphics_close(&graphics) != 0) {
        status = 1;
    }
    if (status != 0) {
        fprintf(stderr, "pool: graphics or input service failed\n");
    }
    return status;
}
