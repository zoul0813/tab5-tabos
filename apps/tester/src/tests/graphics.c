#include <tester/test.h>

#include <tabos/graphics.h>

#include <stddef.h>

void tester_test_graphics(tester_context_t* context)
{
    tabos_graphics_t graphics = {0};
    const int opened          = tabos_graphics_open(&graphics);
    tester_expect(context, opened == 0, "graphics context opens");
    if (opened != 0) {
        return;
    }

    tester_expect(context, graphics.width > 0U && graphics.height > 0U, "graphics dimensions available");
    const uint32_t capabilities = tabos_graphics_capabilities(&graphics);
    tester_expect(context, (capabilities & TABOS_GRAPHICS_CAP_QUEUED) != 0U, "graphics command queue available");
    tester_expect(context, (capabilities & TABOS_GRAPHICS_CAP_TRANSFORM) != 0U, "graphics transforms available");
    tester_expect(context, tabos_graphics_clear(&graphics, TABOS_RGB565(0, 0, 32)) == 0, "framebuffer clears");
    tester_expect(context, tabos_graphics_fill_rect(&graphics, -8, -8, 24U, 24U, TABOS_RGB565(255, 0, 0)) == 0,
                  "filled rectangle clips");
    tester_expect(context, tabos_graphics_line(&graphics, 0, 0, 31, 31, TABOS_RGB565(0, 255, 0)) == 0, "line draws");
    tester_expect(context, tabos_graphics_rect(&graphics, 32, 32, 32U, 24U, TABOS_RGB565(0, 128, 255)) == 0,
                  "outline rectangle draws");
    const tabos_color_t pixels[] = {
        TABOS_RGB565(255, 255, 255),
        TABOS_RGB565(0, 0, 0),
        TABOS_RGB565(0, 0, 0),
        TABOS_RGB565(255, 255, 255),
    };
    tester_expect(context, tabos_graphics_blit(&graphics, 64, 64, 2U, 2U, pixels) == 0, "RGB565 bitmap blits");
    const tabos_graphics_blit_options_t transformed = {
        .pixels        = pixels,
        .bitmap_width  = 2U,
        .bitmap_height = 2U,
        .source        = {.width = 2U, .height = 2U},
        .destination   = {.x = 80, .y = 80, .width = 8U, .height = 8U},
        .rotation      = TABOS_GRAPHICS_ROTATE_90,
        .opacity       = 128U,
    };
    tester_expect(context, tabos_graphics_blit_ex(&graphics, &transformed) == 0, "extended transformed bitmap queues");
    tester_expect(context, tabos_graphics_present(&graphics) == 0, "frame presents");
    tester_expect(context, tabos_graphics_close(&graphics) == 0, "graphics closes and restores terminal");

    graphics                   = (tabos_graphics_t) {.width = 320U, .height = 240U};
    const int letterbox_opened = tabos_graphics_open(&graphics);
    tester_expect(context, letterbox_opened == 0, "4:3 scaled graphics context opens");
    if (letterbox_opened != 0) {
        return;
    }
    tester_expect(context,
                  graphics.scale == 3U && graphics.output_x == 160U && graphics.output_y == 0U &&
                      graphics.output_width == 960U && graphics.output_height == 720U && graphics.letterbox_color == 0U,
                  "4:3 canvas is centered with black letterbox");
    tester_expect(context,
                  tabos_graphics_set_letterbox_color(&graphics, TABOS_RGB565(128, 0, 0)) == 0 &&
                      tabos_graphics_present(&graphics) == 0,
                  "letterbox color changes at runtime");
    tester_expect(context, tabos_graphics_close(&graphics) == 0, "letterboxed graphics closes");

    graphics                = (tabos_graphics_t) {.width = 320U, .height = 180U};
    const int scaled_opened = tabos_graphics_open(&graphics);
    tester_expect(context, scaled_opened == 0, "scaled graphics context opens");
    if (scaled_opened != 0) {
        return;
    }
    tester_expect(context,
                  graphics.width == 320U && graphics.height == 180U && graphics.physical_width == 1280U &&
                      graphics.physical_height == 720U && graphics.scale == 4U &&
                      tabos_graphics_pixels(&graphics) != NULL,
                  "scaled graphics dimensions and framebuffer available");
    tester_expect(context, (tabos_graphics_capabilities(&graphics) & TABOS_GRAPHICS_CAP_SCALED_CANVAS) != 0U,
                  "scaled canvas capability available");
    tester_expect(context,
                  tabos_graphics_clear(&graphics, TABOS_RGB565(0, 0, 0)) == 0 &&
                      tabos_graphics_fill_rect(&graphics, 1, 1, 4U, 4U, TABOS_RGB565(255, 0, 0)) == 0 &&
                      tabos_graphics_blit_ex(&graphics, &transformed) == 0 && tabos_graphics_present(&graphics) == 0,
                  "scaled canvas draws and presents");
    tester_expect(context, tabos_graphics_close(&graphics) == 0, "scaled graphics closes and restores terminal");
}
