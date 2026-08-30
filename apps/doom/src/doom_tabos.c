#include "doom_tabos_input.h"
#include "doom_tabos_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <tabos/input.h>
#include <tabos/runtime_time.h>

#include "doomgeneric.h"
#include "i_system.h"

static doom_tabos_input_t input;
static doom_tabos_video_t video;

static void cleanup(void)
{
    (void) doom_tabos_video_close(&video);
}

static void fail(const char* operation)
{
    cleanup();
    fprintf(stderr, "doom: %s failed\n", operation);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    doomgeneric_Create(argc, argv);

    for (;;) {
        doomgeneric_Tick();
    }
}

void DG_Init(void)
{
    doom_tabos_input_init(&input);
    I_AtExit(cleanup, 1);
    if (doom_tabos_video_open(&video) != 0) {
        fail("graphics open");
    }
}

void DG_DrawFrame(void)
{
    if (doom_tabos_video_draw(&video, DG_ScreenBuffer) != 0) {
        fail("frame presentation");
    }
}

void DG_SleepMs(uint32_t milliseconds)
{
    if (tabos_sleep_ms(milliseconds) != 0) {
        fail("sleep");
    }
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t) tabos_monotonic_ms();
}

int DG_GetKey(int* pressed, unsigned char* key)
{
    if (pressed == NULL || key == NULL) {
        return 0;
    }

    if (doom_tabos_input_pop(&input, pressed, key)) {
        return 1;
    }

    tabos_input_event_t event;
    while (tabos_input_poll(&event)) {
        doom_tabos_input_feed(&input, &event);
    }
    return doom_tabos_input_pop(&input, pressed, key) ? 1 : 0;
}

void DG_SetWindowTitle(const char* title)
{
    (void) title;
}
