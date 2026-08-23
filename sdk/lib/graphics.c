#include <tabos/graphics.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>

extern const tabos_elf_api_t *tabos_runtime_api;

#if defined(TABOS_APPLICATION)
_Static_assert(sizeof(void *) == 4U, "TabOS applications require 32-bit pointers");
_Static_assert(sizeof(tabos_graphics_blit_options_t) == 56U,
               "graphics ABI layout changed");
#endif

static int result(int value)
{
    if (value < 0) { errno = -value; return -1; }
    return value;
}

static bool valid(const tabos_graphics_t *graphics)
{
    return graphics != NULL && graphics->open && tabos_runtime_api != NULL;
}

int tabos_graphics_open(tabos_graphics_t *graphics)
{
    if (graphics == NULL || tabos_runtime_api == NULL || tabos_runtime_api->graphics_open == NULL) {
        errno = graphics == NULL ? EINVAL : ENOSYS;
        return -1;
    }
    uint32_t width = 0U, height = 0U;
    if (result(tabos_runtime_api->graphics_open(&width, &height)) < 0) return -1;
    *graphics = (tabos_graphics_t){.width = width, .height = height, .open = true};
    return 0;
}

int tabos_graphics_clear(tabos_graphics_t *graphics, tabos_color_t color)
{
    if (!valid(graphics) || tabos_runtime_api->graphics_clear == NULL) {
        errno = valid(graphics) ? ENOSYS : EINVAL; return -1;
    }
    return result(tabos_runtime_api->graphics_clear(color));
}
int tabos_graphics_fill_rect(tabos_graphics_t *graphics, int32_t x, int32_t y,
                             uint32_t width, uint32_t height, tabos_color_t color)
{
    if (!valid(graphics) || tabos_runtime_api->graphics_fill_rect == NULL) {
        errno = valid(graphics) ? ENOSYS : EINVAL; return -1;
    }
    return result(tabos_runtime_api->graphics_fill_rect(x, y, width, height, color));
}

int tabos_graphics_pixel(tabos_graphics_t *graphics, int32_t x, int32_t y,
                         tabos_color_t color)
{
    return tabos_graphics_fill_rect(graphics, x, y, 1U, 1U, color);
}

int tabos_graphics_line(tabos_graphics_t *graphics, int32_t x0, int32_t y0,
                        int32_t x1, int32_t y1, tabos_color_t color)
{
    int32_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 >= y0 ? y0 - y1 : y1 - y0;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        if (tabos_graphics_pixel(graphics, x0, y0, color) != 0) return -1;
        if (x0 == x1 && y0 == y1) return 0;
        const int32_t twice_error = error * 2;
        if (twice_error >= dy) { error += dy; x0 += sx; }
        if (twice_error <= dx) { error += dx; y0 += sy; }
    }
}

int tabos_graphics_rect(tabos_graphics_t *graphics, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, tabos_color_t color)
{
    if (width == 0U || height == 0U) return 0;
    if (tabos_graphics_fill_rect(graphics, x, y, width, 1U, color) != 0 ||
        tabos_graphics_fill_rect(graphics, x, y + (int32_t)height - 1, width, 1U, color) != 0 ||
        tabos_graphics_fill_rect(graphics, x, y, 1U, height, color) != 0 ||
        tabos_graphics_fill_rect(graphics, x + (int32_t)width - 1, y, 1U, height, color) != 0)
        return -1;
    return 0;
}
int tabos_graphics_blit(tabos_graphics_t *graphics, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, const tabos_color_t *pixels)
{
    if (!valid(graphics) || pixels == NULL) { errno = EINVAL; return -1; }
    if (tabos_runtime_api->graphics_blit == NULL) { errno = ENOSYS; return -1; }
    return result(tabos_runtime_api->graphics_blit(x, y, width, height, pixels));
}

uint32_t tabos_graphics_capabilities(const tabos_graphics_t *graphics)
{
    if (!valid(graphics) || tabos_runtime_api->graphics_capabilities == NULL) return 0U;
    return tabos_runtime_api->graphics_capabilities();
}

int tabos_graphics_blit_ex(tabos_graphics_t *graphics,
                           const tabos_graphics_blit_options_t *options)
{
    if (!valid(graphics) || options == NULL || options->pixels == NULL) {
        errno = EINVAL; return -1;
    }
    if (tabos_runtime_api->graphics_blit_ex == NULL) { errno = ENOSYS; return -1; }
    return result(tabos_runtime_api->graphics_blit_ex(options));
}
int tabos_graphics_present(tabos_graphics_t *graphics)
{
    if (!valid(graphics)) { errno = EINVAL; return -1; }
    if (tabos_runtime_api->graphics_present == NULL) { errno = ENOSYS; return -1; }
    return result(tabos_runtime_api->graphics_present());
}
int tabos_graphics_close(tabos_graphics_t *graphics)
{
    if (!valid(graphics)) { errno = EINVAL; return -1; }
    if (tabos_runtime_api->graphics_close == NULL) { errno = ENOSYS; return -1; }
    const int value = result(tabos_runtime_api->graphics_close());
    if (value == 0) graphics->open = false;
    return value;
}
