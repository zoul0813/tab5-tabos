#include <tabos/platform/platform.h>

#include <stddef.h>

static tab_pixel_t pixels[TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT];

bool tab_platform_display_init(tab_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }

    *framebuffer = (tab_framebuffer_t){
        .pixels = pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool tab_platform_display_present(const tab_framebuffer_t *framebuffer)
{
    return framebuffer != NULL && framebuffer->pixels == pixels;
}

void tab_platform_display_shutdown(void)
{
}
