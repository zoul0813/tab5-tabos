#include "doom_tabos_video.h"

#include <stdbool.h>
#include <stdlib.h>

static const size_t frame_pixel_count = (size_t) DOOM_TABOS_FRAME_WIDTH * DOOM_TABOS_FRAME_HEIGHT;

int doom_tabos_video_geometry(uint32_t width, uint32_t height, tabos_graphics_rect_t* destination)
{
    if (destination == NULL || width < 4U || height < 3U) {
        return -1;
    }

    uint32_t units;
    if ((uint64_t) width * 3U >= (uint64_t) height * 4U) {
        units = height / 3U;
    } else {
        units = width / 4U;
    }
    if (units == 0U) {
        return -1;
    }

    const uint32_t output_width  = units * 4U;
    const uint32_t output_height = units * 3U;
    *destination                 = (tabos_graphics_rect_t) {
                        .x      = (int32_t) ((width - output_width) / 2U),
                        .y      = (int32_t) ((height - output_height) / 2U),
                        .width  = output_width,
                        .height = output_height,
    };
    return 0;
}

int doom_tabos_convert_rgb565(tabos_color_t* destination, const uint32_t* source, size_t pixel_count)
{
    if (destination == NULL || source == NULL || pixel_count != frame_pixel_count) {
        return -1;
    }

    for (size_t index = 0U; index < pixel_count; ++index) {
        const uint32_t pixel = source[index];
        destination[index] =
            (tabos_color_t) (((pixel >> 8U) & 0xf800U) | ((pixel >> 5U) & 0x07e0U) | ((pixel >> 3U) & 0x001fU));
    }
    return 0;
}

int doom_tabos_video_open(doom_tabos_video_t* video)
{
    if (video == NULL) {
        return -1;
    }

    *video = (doom_tabos_video_t) {0};
    if (tabos_graphics_open(&video->graphics) != 0) {
        return -1;
    }
    if (doom_tabos_video_geometry(video->graphics.width, video->graphics.height, &video->destination) != 0) {
        (void) tabos_graphics_close(&video->graphics);
        return -1;
    }

    video->pixels = malloc(frame_pixel_count * sizeof(*video->pixels));
    if (video->pixels == NULL) {
        (void) tabos_graphics_close(&video->graphics);
        return -1;
    }
    return 0;
}

int doom_tabos_video_draw(doom_tabos_video_t* video, const uint32_t* source)
{
    if (video == NULL || !video->graphics.open || video->pixels == NULL ||
        doom_tabos_convert_rgb565(video->pixels, source, frame_pixel_count) != 0) {
        return -1;
    }
    if (tabos_graphics_clear(&video->graphics, 0U) != 0) {
        return -1;
    }

    const tabos_graphics_blit_options_t blit = {
        .pixels        = video->pixels,
        .bitmap_width  = DOOM_TABOS_FRAME_WIDTH,
        .bitmap_height = DOOM_TABOS_FRAME_HEIGHT,
        .source        = {.width = DOOM_TABOS_FRAME_WIDTH, .height = DOOM_TABOS_FRAME_HEIGHT},
        .destination   = video->destination,
        .opacity       = 255U,
    };
    if (tabos_graphics_blit_ex(&video->graphics, &blit) != 0) {
        return -1;
    }
    return tabos_graphics_present(&video->graphics);
}

int doom_tabos_video_close(doom_tabos_video_t* video)
{
    if (video == NULL) {
        return -1;
    }

    int result = 0;
    if (video->graphics.open && tabos_graphics_close(&video->graphics) != 0) {
        result = -1;
    }
    free(video->pixels);
    *video = (doom_tabos_video_t) {0};
    return result;
}
