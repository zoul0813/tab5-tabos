#include <tabos/internal/display.h>

static tab_framebuffer_t framebuffer;
static bool display_initialized;

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

bool tab_display_present(void)
{
    return display_initialized && tab_platform_display_present(&framebuffer);
}

tab_framebuffer_t *tab_display_framebuffer(void)
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
