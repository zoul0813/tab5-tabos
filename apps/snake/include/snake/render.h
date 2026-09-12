#ifndef SNAKE_RENDER_H
#define SNAKE_RENDER_H

#include <snake/game.h>
#include <tabos/graphics.h>

enum {
    SNAKE_WIDTH  = 640,
    SNAKE_HEIGHT = 360
};
int snake_render(tabos_graphics_t* graphics, const snake_game_t* game, bool sound_enabled);

#endif
