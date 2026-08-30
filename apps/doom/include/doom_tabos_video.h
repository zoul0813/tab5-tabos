#ifndef DOOM_TABOS_VIDEO_H
#define DOOM_TABOS_VIDEO_H

#include <stddef.h>
#include <stdint.h>

#include <tabos/graphics.h>

enum {
    DOOM_TABOS_FRAME_WIDTH  = 320U,
    DOOM_TABOS_FRAME_HEIGHT = 200U,
};

typedef struct {
        tabos_graphics_t graphics;
        tabos_color_t* pixels;
        tabos_graphics_rect_t destination;
} doom_tabos_video_t;

int doom_tabos_video_geometry(uint32_t width, uint32_t height, tabos_graphics_rect_t* destination);
int doom_tabos_convert_rgb565(tabos_color_t* destination, const uint32_t* source, size_t pixel_count);
int doom_tabos_video_open(doom_tabos_video_t* video);
int doom_tabos_video_draw(doom_tabos_video_t* video, const uint32_t* source);
int doom_tabos_video_close(doom_tabos_video_t* video);

#endif
