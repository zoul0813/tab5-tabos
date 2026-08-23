#include <tester/test.h>

#include <tabos/graphics.h>

void tester_test_graphics(tester_context_t *context)
{
    tabos_graphics_t graphics;
    const int opened = tabos_graphics_open(&graphics);
    tester_expect(context, opened == 0, "graphics context opens");
    if (opened != 0) return;

    tester_expect(context, graphics.width > 0U && graphics.height > 0U,
                  "graphics dimensions available");
    const uint32_t capabilities = tabos_graphics_capabilities(&graphics);
    tester_expect(context, (capabilities & TABOS_GRAPHICS_CAP_QUEUED) != 0U,
                  "graphics command queue available");
    tester_expect(context, (capabilities & TABOS_GRAPHICS_CAP_TRANSFORM) != 0U,
                  "graphics transforms available");
    tester_expect(context, tabos_graphics_clear(&graphics, TABOS_RGB565(0, 0, 32)) == 0,
                  "framebuffer clears");
    tester_expect(context, tabos_graphics_fill_rect(&graphics, -8, -8, 24U, 24U,
                  TABOS_RGB565(255, 0, 0)) == 0, "filled rectangle clips");
    tester_expect(context, tabos_graphics_line(&graphics, 0, 0, 31, 31,
                  TABOS_RGB565(0, 255, 0)) == 0, "line draws");
    tester_expect(context, tabos_graphics_rect(&graphics, 32, 32, 32U, 24U,
                  TABOS_RGB565(0, 128, 255)) == 0, "outline rectangle draws");
    const tabos_color_t pixels[] = {
        TABOS_RGB565(255, 255, 255), TABOS_RGB565(0, 0, 0),
        TABOS_RGB565(0, 0, 0), TABOS_RGB565(255, 255, 255),
    };
    tester_expect(context, tabos_graphics_blit(&graphics, 64, 64, 2U, 2U, pixels) == 0,
                  "RGB565 bitmap blits");
    const tabos_graphics_blit_options_t transformed = {
        .pixels = pixels, .bitmap_width = 2U, .bitmap_height = 2U,
        .source = {.width = 2U, .height = 2U},
        .destination = {.x = 80, .y = 80, .width = 8U, .height = 8U},
        .rotation = TABOS_GRAPHICS_ROTATE_90, .opacity = 128U,
    };
    tester_expect(context, tabos_graphics_blit_ex(&graphics, &transformed) == 0,
                  "extended transformed bitmap queues");
    tester_expect(context, tabos_graphics_present(&graphics) == 0, "frame presents");
    tester_expect(context, tabos_graphics_close(&graphics) == 0,
                  "graphics closes and restores terminal");
}
