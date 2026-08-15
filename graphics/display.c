#include <tabos/internal/display.h>

static platform_framebuffer_t framebuffer;
static bool display_initialized;

bool display_init(void)
{
    if (display_initialized) {
        return true;
    }

    if (!platform_display_init(&framebuffer)) {
        return false;
    }

    if (framebuffer.pixels == NULL || framebuffer.width != TABOS_DISPLAY_WIDTH ||
        framebuffer.height != TABOS_DISPLAY_HEIGHT ||
        framebuffer.stride_pixels < TABOS_DISPLAY_WIDTH) {
        platform_display_shutdown();
        framebuffer = (platform_framebuffer_t){0};
        return false;
    }

    display_initialized = true;
    return true;
}

bool display_present(void)
{
    return display_initialized && platform_display_present(&framebuffer);
}

platform_framebuffer_t *display_framebuffer(void)
{
    return display_initialized ? &framebuffer : NULL;
}

void display_shutdown(void)
{
    if (!display_initialized) {
        return;
    }

    platform_display_shutdown();
    framebuffer = (platform_framebuffer_t){0};
    display_initialized = false;
}
