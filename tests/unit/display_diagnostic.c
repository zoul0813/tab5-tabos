#include <tabos/internal/display.h>
#include <tabos/platform/display_transform.h>

#include <stddef.h>

enum {
    RGB565_BLACK = 0x0000,
    RGB565_BLUE = 0x001f,
    RGB565_GREEN = 0x07e0,
    RGB565_CYAN = 0x07ff,
    RGB565_RED = 0xf800,
    RGB565_MAGENTA = 0xf81f,
    RGB565_YELLOW = 0xffe0,
    RGB565_WHITE = 0xffff,
};

static tab_pixel_t pixel_at(const tab_framebuffer_t *framebuffer, size_t x, size_t y)
{
    return framebuffer->pixels[(y * framebuffer->stride_pixels) + x];
}

int main(void)
{
    tab_pixel_t source_pixels[] = {
        1, 2, 3,
        4, 5, 6,
    };
    tab_pixel_t rotated_pixels[6] = {0};
    const tab_framebuffer_t source = {
        .pixels = source_pixels,
        .width = 3,
        .height = 2,
        .stride_pixels = 3,
    };
    const tab_pixel_t expected_rotation[] = {
        4, 1,
        5, 2,
        6, 3,
    };
    const tab_pixel_t expected_counter_rotation[] = {
        3, 6,
        2, 5,
        1, 4,
    };

    if (!tab_framebuffer_rotate_clockwise(&source, rotated_pixels, 2, 3)) {
        return 1;
    }
    for (size_t index = 0; index < sizeof(rotated_pixels) / sizeof(rotated_pixels[0]); ++index) {
        if (rotated_pixels[index] != expected_rotation[index]) {
            return 1;
        }
    }

    if (!tab_framebuffer_rotate_counter_clockwise(&source, rotated_pixels, 2, 3)) {
        return 1;
    }
    for (size_t index = 0; index < sizeof(rotated_pixels) / sizeof(rotated_pixels[0]); ++index) {
        if (rotated_pixels[index] != expected_counter_rotation[index]) {
            return 1;
        }
    }

    if (!tab_display_init()) {
        return 1;
    }

    tab_display_render_diagnostic();
    if (!tab_display_present()) {
        return 1;
    }

    const tab_framebuffer_t *framebuffer = tab_display_framebuffer();
    if (framebuffer == NULL || framebuffer->width != TABOS_DISPLAY_WIDTH ||
        framebuffer->height != TABOS_DISPLAY_HEIGHT ||
        framebuffer->stride_pixels != TABOS_DISPLAY_WIDTH) {
        return 1;
    }

    if (pixel_at(framebuffer, 16, 16) != RGB565_WHITE ||
        pixel_at(framebuffer, TABOS_DISPLAY_WIDTH - 16, 16) != RGB565_RED ||
        pixel_at(framebuffer, 16, TABOS_DISPLAY_HEIGHT - 16) != RGB565_GREEN ||
        pixel_at(framebuffer, TABOS_DISPLAY_WIDTH - 16, TABOS_DISPLAY_HEIGHT - 16) != RGB565_BLUE) {
        return 1;
    }

    if (pixel_at(framebuffer, 100, 60) != RGB565_RED ||
        pixel_at(framebuffer, 300, 60) != RGB565_GREEN ||
        pixel_at(framebuffer, 500, 60) != RGB565_BLUE ||
        pixel_at(framebuffer, 700, 60) != RGB565_CYAN ||
        pixel_at(framebuffer, 900, 60) != RGB565_MAGENTA ||
        pixel_at(framebuffer, 1150, 60) != RGB565_YELLOW ||
        pixel_at(framebuffer, 100, 300) != RGB565_BLACK) {
        return 1;
    }

    tab_display_shutdown();
    return 0;
}
