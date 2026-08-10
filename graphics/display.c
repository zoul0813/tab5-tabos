#include <tabos/internal/display.h>

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

static tab_framebuffer_t framebuffer;
static bool display_initialized;

static void fill_rect(int x, int y, int width, int height, tab_pixel_t color)
{
    for (int row = y; row < y + height; ++row) {
        tab_pixel_t *pixels = framebuffer.pixels + ((size_t)row * framebuffer.stride_pixels);
        for (int column = x; column < x + width; ++column) {
            pixels[column] = color;
        }
    }
}

static tab_pixel_t grayscale(unsigned int level)
{
    const unsigned int red = (level * 31U) / 255U;
    const unsigned int green = (level * 63U) / 255U;
    const unsigned int blue = (level * 31U) / 255U;
    return (tab_pixel_t)((red << 11U) | (green << 5U) | blue);
}

bool tab_display_init(void)
{
    if (display_initialized) {
        return true;
    }

    if (!tab_platform_display_init(&framebuffer)) {
        return false;
    }

    if (framebuffer.pixels == NULL || framebuffer.width != TABOS_DISPLAY_WIDTH ||
        framebuffer.height != TABOS_DISPLAY_HEIGHT ||
        framebuffer.stride_pixels < TABOS_DISPLAY_WIDTH) {
        tab_platform_display_shutdown();
        framebuffer = (tab_framebuffer_t){0};
        return false;
    }

    display_initialized = true;
    return true;
}

void tab_display_render_diagnostic(void)
{
    static const tab_pixel_t bars[] = {
        RGB565_RED,
        RGB565_GREEN,
        RGB565_BLUE,
        RGB565_CYAN,
        RGB565_MAGENTA,
        RGB565_YELLOW,
    };

    if (!display_initialized) {
        return;
    }

    fill_rect(0, 0, TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT, RGB565_BLACK);

    const int bar_width = TABOS_DISPLAY_WIDTH / (int)(sizeof(bars) / sizeof(bars[0]));
    for (size_t index = 0; index < sizeof(bars) / sizeof(bars[0]); ++index) {
        const int x = (int)index * bar_width;
        const int width = index + 1U == sizeof(bars) / sizeof(bars[0])
            ? TABOS_DISPLAY_WIDTH - x
            : bar_width;
        fill_rect(x, 0, width, 120, bars[index]);
    }

    for (int x = 0; x < TABOS_DISPLAY_WIDTH; ++x) {
        const unsigned int level = ((unsigned int)x * 255U) / (TABOS_DISPLAY_WIDTH - 1U);
        fill_rect(x, 120, 1, 80, grayscale(level));
    }

    fill_rect(0, 0, TABOS_DISPLAY_WIDTH, 4, RGB565_WHITE);
    fill_rect(0, TABOS_DISPLAY_HEIGHT - 4, TABOS_DISPLAY_WIDTH, 4, RGB565_WHITE);
    fill_rect(0, 0, 4, TABOS_DISPLAY_HEIGHT, RGB565_WHITE);
    fill_rect(TABOS_DISPLAY_WIDTH - 4, 0, 4, TABOS_DISPLAY_HEIGHT, RGB565_WHITE);

    fill_rect(4, 4, 28, 28, RGB565_WHITE);
    fill_rect(TABOS_DISPLAY_WIDTH - 32, 4, 28, 28, RGB565_RED);
    fill_rect(4, TABOS_DISPLAY_HEIGHT - 32, 28, 28, RGB565_GREEN);
    fill_rect(TABOS_DISPLAY_WIDTH - 32, TABOS_DISPLAY_HEIGHT - 32, 28, 28, RGB565_BLUE);
}

bool tab_display_present(void)
{
    return display_initialized && tab_platform_display_present(&framebuffer);
}

const tab_framebuffer_t *tab_display_framebuffer(void)
{
    return display_initialized ? &framebuffer : NULL;
}

void tab_display_shutdown(void)
{
    if (!display_initialized) {
        return;
    }

    tab_platform_display_shutdown();
    framebuffer = (tab_framebuffer_t){0};
    display_initialized = false;
}
