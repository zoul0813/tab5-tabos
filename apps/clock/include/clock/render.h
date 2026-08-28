#ifndef TABOS_CLOCK_RENDER_H
#define TABOS_CLOCK_RENDER_H

#include <tabos/graphics.h>

#include <time.h>

enum {
    CLOCK_WIDTH  = 640,
    CLOCK_HEIGHT = 360,
};

int clock_render(tabos_graphics_t* graphics, const struct tm* calendar);

#endif
