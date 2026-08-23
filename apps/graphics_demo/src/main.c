#include <tabos/graphics.h>
#include <tabos/runtime_time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    tabos_graphics_t graphics;
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "graphics-demo: open failed (errno %d)\n", errno);
        return 1;
    }

    const int original_flags = fcntl(STDIN_FILENO, F_GETFL);
    (void)fcntl(STDIN_FILENO, F_SETFL, original_flags | O_NONBLOCK);
    int32_t x = (int32_t)graphics.width / 2 - 32;
    int32_t y = (int32_t)graphics.height / 2 - 32;
    tabos_color_t sprite[16U * 16U];
    for (uint32_t row = 0U; row < 16U; ++row)
        for (uint32_t column = 0U; column < 16U; ++column)
            sprite[row * 16U + column] = ((row / 4U) + (column / 4U)) % 2U == 0U
                ? TABOS_RGB565(255, 255, 255) : TABOS_RGB565(255, 64, 96);
    bool running = true;
    tabos_graphics_rotation_t rotation = TABOS_GRAPHICS_ROTATE_0;

    while (running) {
        char input[8];
        const ssize_t count = read(STDIN_FILENO, input, sizeof(input));
        for (ssize_t index = 0; index < count; ++index) {
            switch (input[index]) {
                case 'w': case 'W': y -= 8; break;
                case 's': case 'S': y += 8; break;
                case 'a': case 'A': x -= 8; break;
                case 'd': case 'D': x += 8; break;
                case 'q': case 'Q': running = false; break;
                case 'r': case 'R': rotation = (tabos_graphics_rotation_t)((rotation + 1) % 4); break;
                default: break;
            }
        }
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > (int32_t)graphics.width - 64) x = (int32_t)graphics.width - 64;
        if (y > (int32_t)graphics.height - 64) y = (int32_t)graphics.height - 64;

        (void)tabos_graphics_clear(&graphics, TABOS_RGB565(8, 16, 32));
        (void)tabos_graphics_fill_rect(&graphics, 24, 24,
            graphics.width - 48U, graphics.height - 48U, TABOS_RGB565(20, 40, 72));
        (void)tabos_graphics_fill_rect(&graphics, x, y, 64U, 64U, TABOS_RGB565(255, 176, 32));
        (void)tabos_graphics_fill_rect(&graphics, x + 12, y + 12, 40U, 40U,
                                       TABOS_RGB565(48, 128, 255));
        const tabos_graphics_blit_options_t transformed = {
            .pixels = sprite, .bitmap_width = 16U, .bitmap_height = 16U,
            .source = {.width = 16U, .height = 16U},
            .destination = {.x = x + 16, .y = y + 16, .width = 32U, .height = 32U},
            .rotation = rotation, .opacity = 192U,
            .color_key_enabled = true,
            .color_key_low = TABOS_RGB565(255, 255, 255),
            .color_key_high = TABOS_RGB565(255, 255, 255),
        };
        (void)tabos_graphics_blit_ex(&graphics, &transformed);
        (void)tabos_graphics_present(&graphics);
        (void)tabos_sleep_ms(16U);
    }

    if (original_flags >= 0) (void)fcntl(STDIN_FILENO, F_SETFL, original_flags);
    return tabos_graphics_close(&graphics) == 0 ? 0 : 1;
}
