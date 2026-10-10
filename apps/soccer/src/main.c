#include <soccer/game.h>
#include <soccer/render.h>
#include <soccer/sound.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <unistd.h>

static unsigned int key_bit(tabos_key_t key)
{
    switch (key) {
        case TABOS_KEY_UP: return 1U;
        case TABOS_KEY_W: return 2U;
        case TABOS_KEY_DOWN: return 4U;
        case TABOS_KEY_S: return 8U;
        case TABOS_KEY_LEFT: return 16U;
        case TABOS_KEY_A: return 32U;
        case TABOS_KEY_RIGHT: return 64U;
        case TABOS_KEY_D: return 128U;
        case TABOS_KEY_K: return 256U;
        case TABOS_KEY_J: return 512U;
        default: return 0U;
    }
}
static void new_match(soccer_game_t* game)
{
    const soccer_difficulty_t difficulty = game->difficulty;
    soccer_game_reset(game);
    game->difficulty = difficulty;
}

int main(int argc, char** argv)
{
    bool profiling = false;
    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--profile") == 0) {
            profiling = true;
        } else {
            fprintf(stderr, "Usage: soccer [--profile]\n");
            return 1;
        }
    }
    uint64_t active_ms = 0U, frames = 0U, steps = 0U, work_ms = 0U;
    uint64_t worst_ms = 0U, slow_frames = 0U, discarded_ms = 0U;

    uint32_t tty = 0U;
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty) != 0 ||
        ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, tty | (uint32_t) TABOS_TTY_MODE_RAW_INPUT) != 0) {
        fprintf(stderr, "soccer: raw keyboard unavailable\n");
        return 1;
    }
    tabos_graphics_t graphics = {.width = 320U, .height = 240U};
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "soccer: graphics unavailable\n");
        return 1;
    }
    tabos_wait_item_t wait = {.source = tabos_input_wait_source(), .events = TABOS_WAIT_READABLE};
    soccer_sound_t sound   = {.stream = TABOS_AUDIO_STREAM_INVALID};
    soccer_game_t game;
    soccer_game_init(&game);
    soccer_visuals_t visuals;
    soccer_visuals_reset(&visuals, &game);
    soccer_camera_t camera;
    soccer_camera_reset(&camera, &game);
    unsigned int held = 0U;
    bool pass         = false;
    bool shoot = false, running = true, dirty = true;
    uint64_t previous    = tabos_monotonic_ms();
    uint32_t accumulator = 0U;
    int status           = wait.source == TABOS_WAIT_SOURCE_INVALID ? 1 : 0;
    while (running && status == 0) {
        soccer_event_t effect = SOCCER_EVENT_NONE;
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            if (event.type == TABOS_INPUT_KEY_UP) {
                held &= ~key_bit(event.key);
            }
            if (event.type != TABOS_INPUT_KEY_DOWN || event.repeat) {
                continue;
            }
            if ((game.mode == SOCCER_TITLE || game.mode == SOCCER_FULL_TIME) &&
                (event.key == TABOS_KEY_LEFT || event.key == TABOS_KEY_A || event.key == TABOS_KEY_RIGHT ||
                 event.key == TABOS_KEY_D)) {
                const int change = event.key == TABOS_KEY_LEFT || event.key == TABOS_KEY_A ? -1 : 1;
                game.difficulty  = (soccer_difficulty_t) (((int) game.difficulty + change + 3) % 3);
                dirty            = true;
                continue;
            }
            held |= key_bit(event.key);
            if (event.key == TABOS_KEY_Q || event.key == TABOS_KEY_ESCAPE) {
                running = false;
                break;
            }
            if (event.key == TABOS_KEY_M) {
                soccer_sound_toggle(&sound);
                if (!sound.muted && !game.paused && game.mode != SOCCER_TITLE) {
                    soccer_sound_prepare(&sound);
                }
                /* Exclude codec startup/shutdown from simulation catch-up. */
                previous    = tabos_monotonic_ms();
                accumulator = 0U;
                dirty       = true;
            }
            bool transition = false;
            if (event.key == TABOS_KEY_ENTER && (game.mode == SOCCER_TITLE || game.mode == SOCCER_FULL_TIME)) {
                new_match(&game);
                soccer_visuals_reset(&visuals, &game);
                soccer_camera_reset(&camera, &game);
                transition = true;
            } else if (event.key == TABOS_KEY_P && game.mode != SOCCER_TITLE && game.mode != SOCCER_FULL_TIME) {
                game.paused = !game.paused;
                transition  = true;
            } else if (event.key == TABOS_KEY_R && game.mode != SOCCER_TITLE) {
                new_match(&game);
                soccer_visuals_reset(&visuals, &game);
                soccer_camera_reset(&camera, &game);
                transition = true;
            } else if (event.key == TABOS_KEY_K && (game.mode == SOCCER_PLAY || game.mode == SOCCER_RESTART) &&
                       !game.paused) {
                shoot = true;
            }
            if ((game.mode == SOCCER_PLAY || game.mode == SOCCER_RESTART) && !game.paused) {
                if (event.key == TABOS_KEY_J) {
                    pass = true;
                }
            }
            if (transition) {
                soccer_sound_stop(&sound);
                if (!game.paused && game.mode != SOCCER_TITLE) {
                    soccer_sound_prepare(&sound);
                }
                game.shot_charge = 0U;
                game.pass_charge = 0U;
                pass             = false;
                held             = 0U;
                shoot            = false;
                accumulator      = 0U;
                previous         = tabos_monotonic_ms();
                dirty            = true;
            }
        }
        if (!running) {
            break;
        }
        const uint64_t now     = tabos_monotonic_ms();
        const uint64_t elapsed = now - previous;
        previous               = now;
        const bool active      = game.mode != SOCCER_TITLE && game.mode != SOCCER_FULL_TIME && !game.paused;
        if (profiling && active) {
            active_ms += elapsed;
            if (elapsed > 100U) {
                discarded_ms += elapsed - 100U;
            }
        }
        if (active) {
            accumulator += (uint32_t) (elapsed > 100U ? 100U : elapsed) * 60U;
            while (accumulator >= 1000U) {
                const soccer_input_t input = {
                    .dx         = ((held & 192U) != 0U) - ((held & 48U) != 0U),
                    .dy         = ((held & 12U) != 0U) - ((held & 3U) != 0U),
                    .shoot      = shoot,
                    .shoot_held = (held & 256U) != 0U,
                    .pass_held  = (held & 512U) != 0U,
                    .pass       = pass,
                };
                const soccer_mode_t mode = game.mode;
                soccer_game_arcade_step(&game, input);
                soccer_visuals_step(&visuals, &game);
                if (profiling) {
                    ++steps;
                }
                if (game.event > effect) {
                    effect = game.event;
                }
                if (mode != game.mode) {
                    if (game.mode == SOCCER_KICKOFF || game.mode == SOCCER_RESTART) {
                        soccer_camera_reset(&camera, &game);
                    }
                    /* Keep a direction held across the kickoff whistle. Action
                       keys still require a fresh press once play begins. */
                    held             = mode == SOCCER_KICKOFF && game.mode == SOCCER_PLAY ? held & 255U : 0U;
                    game.shot_charge = 0U;
                    game.pass_charge = 0U;
                }
                soccer_camera_step(&camera, &game);
                pass         = false;
                shoot        = false;
                accumulator -= 1000U;
                dirty        = true;
            }
        } else {
            accumulator = 0U;
        }
        soccer_sound_play(&sound, effect);
        if (dirty && soccer_render_trial(&graphics, &game, &camera, sound.muted, &visuals) != 0) {
            status = 1;
            break;
        }
        if (profiling && active && dirty) {
            const uint64_t work = tabos_monotonic_ms() - now;
            ++frames;
            work_ms += work;
            if (work > worst_ms) {
                worst_ms = work;
            }
            if (work >= 17U) {
                ++slow_frames;
            }
        }
        dirty            = false;
        uint32_t timeout = TABOS_WAIT_TIMEOUT_INFINITE;
        if (active) {
            /* Account for time spent presenting, which may already wait for VSYNC. */
            const uint64_t spent = tabos_monotonic_ms() - previous;
            const uint32_t until = (1000U - accumulator + 59U) / 60U;
            timeout              = spent >= until ? 0U : until - (uint32_t) spent;
        }
        if (tabos_wait(&wait, 1U, timeout) < 0) {
            status = 1;
        }
    }
    soccer_sound_close(&sound);
    if (sound.error != 0) {
        fprintf(stderr, "soccer: audio unavailable: %s\n", strerror(sound.error));
    }
    if (tabos_graphics_close(&graphics) != 0) {
        status = 1;
    }
    if (status != 0) {
        fprintf(stderr, "soccer: graphics or input service failed\n");
    }
    if (profiling) {
        printf("Soccer profile (active play, kickoff and restarts):\n"
               "  elapsed: %" PRIu64 " ms; frames: %" PRIu64 "; steps: %" PRIu64 "\n"
               "  update/audio/present: total %" PRIu64 " ms; worst %" PRIu64 " ms\n"
               "  frames >=17 ms: %" PRIu64 "; catch-up discarded: %" PRIu64 " ms\n",
               active_ms, frames, steps, work_ms, worst_ms, slow_frames, discarded_ms);
    }
    return status;
}
