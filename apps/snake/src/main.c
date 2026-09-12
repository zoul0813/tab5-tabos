#include <snake/game.h>
#include <snake/render.h>
#include <snake/sound.h>

#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <unistd.h>

static bool handle_key(snake_game_t* game, snake_sound_t* sound, tabos_key_t key, uint64_t now)
{
    switch (key) {
        case TABOS_KEY_Q:
        case TABOS_KEY_ESCAPE: return false;
        case TABOS_KEY_ENTER:
        case TABOS_KEY_SPACE:
            if (game->mode == SNAKE_TITLE || game->mode == SNAKE_OVER || game->mode == SNAKE_WON) {
                snake_game_start(game, now);
                snake_sound_play(sound, SNAKE_SOUND_START);
            } else {
                snake_game_pause(game, now);
                snake_sound_stop(sound);
            }
            break;
        case TABOS_KEY_R:
            snake_game_start(game, now);
            snake_sound_play(sound, SNAKE_SOUND_START);
            break;
        case TABOS_KEY_P:
            snake_game_pause(game, now);
            snake_sound_stop(sound);
            break;
        case TABOS_KEY_M: snake_sound_toggle(sound); break;
        case TABOS_KEY_W:
        case TABOS_KEY_UP: (void) snake_game_turn(game, SNAKE_UP); break;
        case TABOS_KEY_D:
        case TABOS_KEY_RIGHT: (void) snake_game_turn(game, SNAKE_RIGHT); break;
        case TABOS_KEY_S:
        case TABOS_KEY_DOWN: (void) snake_game_turn(game, SNAKE_DOWN); break;
        case TABOS_KEY_A:
        case TABOS_KEY_LEFT: (void) snake_game_turn(game, SNAKE_LEFT); break;
        default: break;
    }
    return true;
}

int main(void)
{
    uint32_t tty_mode = 0U;
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty_mode) != 0 ||
        ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, tty_mode | (uint32_t) TABOS_TTY_MODE_RAW_INPUT) != 0) {
        fprintf(stderr, "snake: raw input failed: %d\n", errno);
        return 1;
    }
    tabos_graphics_t graphics = {.width = SNAKE_WIDTH, .height = SNAKE_HEIGHT};
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "snake: graphics open failed: %d\n", errno);
        return 1;
    }
    tabos_wait_item_t input = {.source = tabos_input_wait_source(), .events = TABOS_WAIT_READABLE};
    int status              = 0;
    snake_sound_t sound     = {.stream = TABOS_AUDIO_STREAM_INVALID};
    if (input.source == TABOS_WAIT_SOURCE_INVALID) {
        status = 1;
    } else {
        snake_game_t game;
        snake_game_init(&game, (uint32_t) tabos_monotonic_ms());
        bool running = true;
        bool dirty   = true;
        while (running) {
            tabos_input_event_t event;
            while (tabos_input_poll(&event)) {
                if (event.type == TABOS_INPUT_KEY_DOWN && !event.repeat) {
                    running = handle_key(&game, &sound, event.key, tabos_monotonic_ms());
                    dirty   = true;
                    if (!running) {
                        break;
                    }
                }
            }
            if (!running) {
                break;
            }
            const snake_mode_t previous_mode = game.mode;
            const uint16_t previous_length   = game.length;
            dirty                            = snake_game_update(&game, tabos_monotonic_ms()) || dirty;
            if (game.mode == SNAKE_WON && previous_mode != SNAKE_WON) {
                snake_sound_play(&sound, SNAKE_SOUND_WIN);
            } else if (game.mode == SNAKE_OVER && previous_mode != SNAKE_OVER) {
                snake_sound_play(&sound, SNAKE_SOUND_LOSE);
            } else if (game.length > previous_length) {
                snake_sound_play(&sound, SNAKE_SOUND_EAT);
            }
            if (dirty && snake_render(&graphics, &game, !sound.muted) != 0) {
                status = 1;
                break;
            }
            dirty            = false;
            uint32_t timeout = TABOS_WAIT_TIMEOUT_INFINITE;
            if (game.mode == SNAKE_PLAYING) {
                const uint64_t now = tabos_monotonic_ms();
                timeout            = now >= game.deadline ? 0U : (uint32_t) (game.deadline - now);
            }
            if (tabos_wait(&input, 1U, timeout) < 0) {
                status = 1;
                break;
            }
        }
    }
    snake_sound_close(&sound);
    if (tabos_graphics_close(&graphics) != 0) {
        status = 1;
    }
    if (status != 0) {
        fprintf(stderr, "snake: graphics or input service failed\n");
    }
    return status;
}
