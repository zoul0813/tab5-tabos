#include "doom_tabos_input.h"
#include "doom_tabos_storage.h"
#include "doom_tabos_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <tabos/input.h>
#include <tabos/runtime_time.h>

#include "doomgeneric.h"
#include "i_system.h"

static doom_tabos_input_t input;
static doom_tabos_launch_t launch;
static doom_tabos_video_t video;

static void cleanup(void)
{
    (void) doom_tabos_video_close(&video);
    doom_tabos_storage_release(&launch);
}

static void fail(const char* operation)
{
    cleanup();
    fprintf(stderr, "doom: %s failed\n", operation);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    const doom_tabos_storage_status_t storage_status = doom_tabos_storage_prepare(argc, argv, &launch);
    if (storage_status != DOOM_TABOS_STORAGE_OK) {
        if (storage_status == DOOM_TABOS_STORAGE_DIRECTORY_FAILED) {
            fprintf(stderr, "doom: cannot use %s\n", DOOM_TABOS_GAME_DIRECTORY);
        } else if (storage_status == DOOM_TABOS_STORAGE_IWAD_NOT_FOUND) {
            fprintf(stderr, "doom: no usable IWAD found in %s\n", DOOM_TABOS_GAME_DIRECTORY);
            fprintf(stderr, "install doom1.wad, doom.wad, doom2.wad, freedoom1.wad, or freedoom2.wad there\n");
            fprintf(stderr, "or use -iwad <path>\n");
        } else {
            fprintf(stderr, "doom: startup failed\n");
        }
        doom_tabos_storage_release(&launch);
        return EXIT_FAILURE;
    }

    doomgeneric_Create(launch.argc, launch.argv);

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
