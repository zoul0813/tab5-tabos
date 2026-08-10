#include <tabos/internal/display.h>
#include <tabos/internal/font.h>
#include <tabos/internal/terminal.h>
#include <tabos/platform/display_transform.h>

#include <stddef.h>

static tab_pixel_t pixel_at(const tab_framebuffer_t *framebuffer, size_t x, size_t y)
{
    return framebuffer->pixels[(y * framebuffer->stride_pixels) + x];
}

int main(void)
{
    tab_pixel_t source_pixels[] = {1, 2, 3, 4, 5, 6};
    tab_pixel_t rotated_pixels[6] = {0};
    const tab_framebuffer_t source = {
        .pixels = source_pixels,
        .width = 3,
        .height = 2,
        .stride_pixels = 3,
    };
    const tab_pixel_t expected_rotation[] = {4, 1, 5, 2, 6, 3};
    const tab_pixel_t expected_counter_rotation[] = {3, 6, 2, 5, 1, 4};

    if (!tab_framebuffer_rotate_clockwise(&source, rotated_pixels, 2, 3)) {
        return 1;
    }
    for (size_t index = 0; index < 6U; ++index) {
        if (rotated_pixels[index] != expected_rotation[index]) {
            return 1;
        }
    }
    if (!tab_framebuffer_rotate_counter_clockwise(&source, rotated_pixels, 2, 3)) {
        return 1;
    }
    for (size_t index = 0; index < 6U; ++index) {
        if (rotated_pixels[index] != expected_counter_rotation[index]) {
            return 1;
        }
    }

    if (!tab_display_init()) {
        return 1;
    }
    tab_framebuffer_t *framebuffer = tab_display_framebuffer();
    if (framebuffer == NULL || framebuffer->width != TABOS_DISPLAY_WIDTH ||
        framebuffer->height != TABOS_DISPLAY_HEIGHT) {
        return 1;
    }

    tab_terminal_t terminal;
    if (!tab_terminal_init(&terminal, framebuffer, 2U)) {
        return 1;
    }
    tab_terminal_clear(&terminal);
    tab_terminal_write_line(&terminal, "A");

    if (pixel_at(framebuffer, 4, 0) != 0xffff || pixel_at(framebuffer, 0, 0) != 0x0000 ||
        terminal.row != 1U || terminal.column != 0U) {
        return 1;
    }
    if (!tab_display_present()) {
        return 1;
    }
    tab_display_shutdown();
    return 0;
}
