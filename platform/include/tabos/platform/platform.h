#ifndef TABOS_PLATFORM_PLATFORM_H
#define TABOS_PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TABOS_DISPLAY_WIDTH = 1280,
    TABOS_DISPLAY_HEIGHT = 720,
};

typedef uint16_t tab_pixel_t;

typedef struct {
    tab_pixel_t *pixels;
    size_t width;
    size_t height;
    size_t stride_pixels;
} tab_framebuffer_t;

bool tab_platform_init(bool headless);
int tab_platform_run(void);
void tab_platform_shutdown(void);
const char *tab_platform_name(void);
const char *tab_platform_display_name(void);

bool tab_platform_display_init(tab_framebuffer_t *framebuffer);
bool tab_platform_display_present(const tab_framebuffer_t *framebuffer);
void tab_platform_display_shutdown(void);

#endif
