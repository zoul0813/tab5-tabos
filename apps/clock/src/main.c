#include <clock/render.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tty.h>
#include <time.h>
#include <unistd.h>

static bool exit_requested(void)
{
    bool requested = false;
    tabos_input_event_t event;
    while (tabos_input_poll(&event)) {
        if (event.type == TABOS_INPUT_KEY_DOWN && !event.repeat &&
            (event.key == TABOS_KEY_Q || event.key == TABOS_KEY_ESCAPE)) {
            requested = true;
        }
    }
    return requested;
}

int main(void)
{
    uint32_t tty_mode = 0U;
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty_mode) == 0) {
        (void) ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE,
                     (tty_mode & ~(uint32_t) TABOS_TTY_MODE_SCROLL_KEYS) | (uint32_t) TABOS_TTY_MODE_RAW_INPUT);
    }
    tabos_graphics_t graphics = {
        .width  = CLOCK_WIDTH,
        .height = CLOCK_HEIGHT,
    };
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "clock: graphics open failed: %d\n", errno);
        return 1;
    }

    time_t displayed = (time_t) -1;
    int exit_status  = 0;
    bool running     = true;
    while (running) {
        if (exit_requested()) {
            break;
        }
        const time_t now = time(NULL);
        if (now == (time_t) -1) {
            fprintf(stderr, "clock: cannot read wall clock: %d\n", errno);
            exit_status = 1;
            break;
        }
        if (now != displayed) {
            const struct tm* calendar = gmtime(&now);
            if (calendar == NULL || clock_render(&graphics, calendar) != 0) {
                fprintf(stderr, "clock: display update failed\n");
                exit_status = 1;
                break;
            }
            displayed = now;
        }
        if (tabos_sleep_ms(16U) != 0) {
            exit_status = 1;
            break;
        }
    }
    if (tabos_graphics_close(&graphics) != 0) {
        exit_status = 1;
    }
    return exit_status;
}
