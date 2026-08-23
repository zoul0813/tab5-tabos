#include <tabos/internal/display.h>
#include <tabos/internal/font.h>
#include <tabos/internal/terminal.h>
#include <tabos/platform/display_transform.h>

#include <stddef.h>

static platform_pixel_t pixel_at(const platform_framebuffer_t* framebuffer, size_t x, size_t y)
{
    return framebuffer->pixels[(y * framebuffer->stride_pixels) + x];
}

int main(void)
{
    platform_pixel_t source_pixels[]    = {1, 2, 3, 4, 5, 6};
    platform_pixel_t rotated_pixels[6]  = {0};
    const platform_framebuffer_t source = {
        .pixels        = source_pixels,
        .width         = 3,
        .height        = 2,
        .stride_pixels = 3,
    };
    const platform_pixel_t expected_rotation[]         = {4, 1, 5, 2, 6, 3};
    const platform_pixel_t expected_counter_rotation[] = {3, 6, 2, 5, 1, 4};

    if (font_glyph_index(FONT_GLYPH_COUNT - 1U) != FONT_GLYPH_COUNT - 1U || font_glyph_index(FONT_GLYPH_COUNT) != 0U ||
        font_glyph_index(256U) != 0U || (FONT_GLYPH_COUNT <= 220U && font_glyph_index(220U) != 0U)) {
        return 1;
    }

    if (!platform_framebuffer_rotate_clockwise(&source, rotated_pixels, 2, 3)) {
        return 1;
    }
    for (size_t index = 0; index < 6U; ++index) {
        if (rotated_pixels[index] != expected_rotation[index]) {
            return 1;
        }
    }
    if (!platform_framebuffer_rotate_counter_clockwise(&source, rotated_pixels, 2, 3)) {
        return 1;
    }
    for (size_t index = 0; index < 6U; ++index) {
        if (rotated_pixels[index] != expected_counter_rotation[index]) {
            return 1;
        }
    }

    if (!display_init()) {
        return 1;
    }
    platform_framebuffer_t* framebuffer = display_framebuffer();
    if (framebuffer == NULL || framebuffer->width != TABOS_DISPLAY_WIDTH ||
        framebuffer->height != TABOS_DISPLAY_HEIGHT) {
        return 1;
    }

    terminal_t terminal;
    if (!terminal_init(&terminal, framebuffer, 2U)) {
        return 1;
    }
    if (terminal.columns != 80U || terminal.rows != 24U) {
        return 1;
    }
    terminal_clear(&terminal);
    terminal_write_line(&terminal, "A");

    if (pixel_at(framebuffer, 6, 2) != 0xffff || pixel_at(framebuffer, 0, 0) != 0x0000 || terminal.row != 1U ||
        terminal.column != 0U) {
        return 1;
    }

    terminal_clear(&terminal);
    terminal_write(&terminal, "AB\tC");
    if (terminal.column != 5U || terminal.row != 0U) {
        return 1;
    }
    terminal_write(&terminal, "\b");
    if (terminal.column != 4U || terminal.row != 0U) {
        return 1;
    }
    if (!display_present()) {
        return 1;
    }
    terminal_shutdown(&terminal);
    display_shutdown();
    return 0;
}
