#ifndef TABOS_INTERNAL_RASTER_H
#define TABOS_INTERNAL_RASTER_H

#include <tabos/graphics.h>
#include <tabos/platform/platform.h>

void raster_fill(platform_framebuffer_t *framebuffer, int32_t x, int32_t y,
                 uint32_t width, uint32_t height, tabos_color_t color);
bool raster_blit(platform_framebuffer_t *framebuffer,
                 const tabos_graphics_blit_options_t *options);
void raster_fill_span(platform_pixel_t *destination, size_t count, platform_pixel_t color);
void raster_copy_span(platform_pixel_t *destination, const platform_pixel_t *source,
                      size_t count);

#endif
