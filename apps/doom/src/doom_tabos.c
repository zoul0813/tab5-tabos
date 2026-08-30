// Phase 5 link adapter. Phase 6 replaces these placeholders with TabOS graphics,
// timing, and raw-input behavior.

#include <stdint.h>

#include "doomgeneric.h"

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    for (;;) {
        doomgeneric_Tick();
    }
}

void DG_Init(void)
{
}

void DG_DrawFrame(void)
{
}

void DG_SleepMs(uint32_t milliseconds)
{
    (void)milliseconds;
}

uint32_t DG_GetTicksMs(void)
{
    return 0U;
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    (void)pressed;
    (void)key;
    return 0;
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}
