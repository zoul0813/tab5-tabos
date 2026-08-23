#include <starfall/game.h>
#include <starfall/render.h>
#include <starfall/storage.h>

#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tty.h>
#include <unistd.h>

static void set_held(starfall_input_t *input, tabos_key_t key, bool down)
{
    switch (key) {
        case TABOS_KEY_A: input->left = down; break;
        case TABOS_KEY_S: input->right = down; break;
        case TABOS_KEY_K: input->fire = down; break;
        default: break;
    }
}

int main(void)
{
    uint32_t tty_mode = 0U;
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty_mode) == 0)
        (void)ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE,
                    (tty_mode & ~(uint32_t)TABOS_TTY_MODE_SCROLL_KEYS) |
                        (uint32_t)TABOS_TTY_MODE_RAW_INPUT);
    tabos_graphics_t graphics = {.width = STARFALL_WIDTH, .height = STARFALL_HEIGHT};
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "starfall: graphics open failed: %d\n", errno);
        return 1;
    }
    uint32_t persisted_high_score = starfall_high_score_load();
    starfall_game_t game;
    starfall_game_init(&game, (uint32_t)tabos_monotonic_ms(), persisted_high_score);
    uint64_t previous = tabos_monotonic_ms();
    uint32_t accumulator = 0U;
    bool running = true;
    int exit_status = 0;

    while (running) {
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            if (event.type == TABOS_INPUT_KEY_DOWN || event.type == TABOS_INPUT_KEY_UP)
                set_held(&game.input, event.key, event.type == TABOS_INPUT_KEY_DOWN);
            if (event.type != TABOS_INPUT_KEY_DOWN || event.repeat) continue;
            if (event.key == TABOS_KEY_Q || event.key == TABOS_KEY_ESCAPE) running = false;
            else if (event.key == TABOS_KEY_K &&
                     (game.mode == STARFALL_TITLE || game.mode == STARFALL_GAME_OVER))
                starfall_game_start(&game);
            else if (event.key == TABOS_KEY_P) starfall_game_toggle_pause(&game);
        }

        const uint64_t now = tabos_monotonic_ms();
        uint64_t elapsed = now - previous;
        previous = now;
        if (elapsed > 100U) elapsed = 100U;
        accumulator += (uint32_t)elapsed * 60U;
        unsigned int catch_up = 0U;
        while (accumulator >= 1000U && catch_up < 6U) {
            starfall_game_update(&game);
            accumulator -= 1000U;
            ++catch_up;
        }
        if (catch_up == 6U && accumulator >= 1000U) accumulator = 0U;

        const tabos_color_t border = game.mode == STARFALL_PAUSED
            ? TABOS_RGB565(0, 48, 128)
            : game.tick < game.damage_flash_until ? TABOS_RGB565(192, 0, 24)
            : TABOS_RGB565(0, 0, 0);
        (void)tabos_graphics_set_letterbox_color(&graphics, border);
        if (starfall_render(&graphics, &game) != 0) {
            exit_status = 1;
            running = false;
            break;
        }
        if (game.high_score > persisted_high_score && game.mode == STARFALL_GAME_OVER) {
            if (starfall_high_score_save(game.high_score) == 0)
                persisted_high_score = game.high_score;
        }
    }
    if (game.high_score > persisted_high_score)
        (void)starfall_high_score_save(game.high_score);
    if (tabos_graphics_close(&graphics) != 0) exit_status = 1;
    return exit_status;
}
