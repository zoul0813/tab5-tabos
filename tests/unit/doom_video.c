#include "doom_tabos_video.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum {
    FAILURE_NONE,
    FAILURE_OPEN,
    FAILURE_CLEAR,
    FAILURE_BLIT,
    FAILURE_PRESENT,
    FAILURE_CLOSE,
} failure_t;

static failure_t failure;
static uint32_t display_width  = 1280U;
static uint32_t display_height = 720U;
static unsigned int clear_count;
static unsigned int blit_count;
static unsigned int present_count;
static unsigned int close_count;
static bool blit_valid;

int tabos_graphics_open(tabos_graphics_t* graphics)
{
    if (failure == FAILURE_OPEN) {
        return -1;
    }
    graphics->width           = display_width;
    graphics->height          = display_height;
    graphics->physical_width  = display_width;
    graphics->physical_height = display_height;
    graphics->open            = true;
    return 0;
}

int tabos_graphics_clear(tabos_graphics_t* graphics, tabos_color_t color)
{
    ++clear_count;
    if (!graphics->open || color != 0U || failure == FAILURE_CLEAR) {
        return -1;
    }
    return 0;
}

int tabos_graphics_blit_ex(tabos_graphics_t* graphics, const tabos_graphics_blit_options_t* options)
{
    ++blit_count;
    blit_valid = graphics->open && options != NULL && options->pixels != NULL &&
                 options->bitmap_width == DOOM_TABOS_FRAME_WIDTH && options->bitmap_height == DOOM_TABOS_FRAME_HEIGHT &&
                 options->source.x == 0 && options->source.y == 0 && options->source.width == DOOM_TABOS_FRAME_WIDTH &&
                 options->source.height == DOOM_TABOS_FRAME_HEIGHT && options->destination.x == 160 &&
                 options->destination.y == 0 && options->destination.width == 960U &&
                 options->destination.height == 720U && options->opacity == 255U;
    if (!blit_valid || failure == FAILURE_BLIT) {
        return -1;
    }
    return 0;
}

int tabos_graphics_present(tabos_graphics_t* graphics)
{
    ++present_count;
    if (!graphics->open || failure == FAILURE_PRESENT) {
        return -1;
    }
    return 0;
}

int tabos_graphics_close(tabos_graphics_t* graphics)
{
    ++close_count;
    if (failure == FAILURE_CLOSE) {
        return -1;
    }
    graphics->open = false;
    return 0;
}

static int test_geometry(void)
{
    tabos_graphics_rect_t destination;
    if (doom_tabos_video_geometry(1280U, 720U, &destination) != 0 || destination.x != 160 || destination.y != 0 ||
        destination.width != 960U || destination.height != 720U) {
        return 1;
    }
    if (doom_tabos_video_geometry(800U, 600U, &destination) != 0 || destination.x != 0 || destination.y != 0 ||
        destination.width != 800U || destination.height != 600U) {
        return 1;
    }
    return doom_tabos_video_geometry(3U, 2U, &destination) == -1 && doom_tabos_video_geometry(1280U, 720U, NULL) == -1 ?
               0 :
               1;
}

static int test_conversion(void)
{
    const size_t pixel_count = (size_t) DOOM_TABOS_FRAME_WIDTH * DOOM_TABOS_FRAME_HEIGHT;
    uint32_t* source         = calloc(pixel_count, sizeof(*source));
    tabos_color_t* pixels    = calloc(pixel_count, sizeof(*pixels));
    if (source == NULL || pixels == NULL) {
        free(source);
        free(pixels);
        return 1;
    }

    source[0]        = 0x00ff0000U;
    source[1]        = 0x0000ff00U;
    source[2]        = 0x000000ffU;
    source[3]        = 0x00123456U;
    const int result = doom_tabos_convert_rgb565(pixels, source, pixel_count);
    const bool valid = result == 0 && pixels[0] == 0xf800U && pixels[1] == 0x07e0U && pixels[2] == 0x001fU &&
                       pixels[3] == 0x11aaU && doom_tabos_convert_rgb565(NULL, source, pixel_count) == -1 &&
                       doom_tabos_convert_rgb565(pixels, NULL, pixel_count) == -1 &&
                       doom_tabos_convert_rgb565(pixels, source, pixel_count - 1U) == -1;
    free(source);
    free(pixels);
    return valid ? 0 : 1;
}

static int test_video(void)
{
    const size_t pixel_count = (size_t) DOOM_TABOS_FRAME_WIDTH * DOOM_TABOS_FRAME_HEIGHT;
    uint32_t* source         = calloc(pixel_count, sizeof(*source));
    doom_tabos_video_t video;
    if (source == NULL || doom_tabos_video_open(&video) != 0) {
        free(source);
        return 1;
    }
    if (doom_tabos_video_draw(&video, source) != 0 || clear_count != 1U || blit_count != 1U || present_count != 1U ||
        !blit_valid || doom_tabos_video_draw(&video, NULL) != -1) {
        (void) doom_tabos_video_close(&video);
        free(source);
        return 1;
    }

    failure = FAILURE_CLEAR;
    if (doom_tabos_video_draw(&video, source) != -1) {
        return 1;
    }
    failure = FAILURE_BLIT;
    if (doom_tabos_video_draw(&video, source) != -1) {
        return 1;
    }
    failure = FAILURE_PRESENT;
    if (doom_tabos_video_draw(&video, source) != -1) {
        return 1;
    }
    failure = FAILURE_NONE;
    if (doom_tabos_video_close(&video) != 0 || video.pixels != NULL || video.graphics.open) {
        free(source);
        return 1;
    }

    failure = FAILURE_OPEN;
    if (doom_tabos_video_open(&video) != -1) {
        free(source);
        return 1;
    }
    failure       = FAILURE_NONE;
    display_width = 3U;
    if (doom_tabos_video_open(&video) != -1) {
        free(source);
        return 1;
    }
    display_width = 1280U;
    if (doom_tabos_video_open(&video) != 0) {
        free(source);
        return 1;
    }
    failure = FAILURE_CLOSE;
    if (doom_tabos_video_close(&video) != -1 || video.pixels != NULL || video.graphics.open) {
        free(source);
        return 1;
    }

    free(source);
    return 0;
}

int main(void)
{
    if (test_geometry() != 0 || test_conversion() != 0 || test_video() != 0) {
        return 1;
    }
    return 0;
}
