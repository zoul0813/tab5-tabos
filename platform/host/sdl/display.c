#include "internal.h"

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>
#include <tabos/config/display.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SDL_Renderer* renderer;
static SDL_Texture* texture;
static platform_pixel_t* framebuffer_pixels;
static platform_pixel_t* presented_pixels;
static Uint64 graphics_present_deadline_ns;
static bool renderer_vsync;

bool host_capture_screenshot(void)
{
    if (presented_pixels == NULL || host_is_headless()) {
        return false;
    }
    if (!SDL_CreateDirectory("screenshots")) {
        SDL_Log("Could not create screenshot directory: %s", SDL_GetError());
        return false;
    }

    const time_t now = time(NULL);
    struct tm local_time;
    if (now == (time_t) -1 || localtime_r(&now, &local_time) == NULL) {
        SDL_Log("Could not create screenshot timestamp");
        return false;
    }
    char path[80];
    if (strftime(path, sizeof(path), "screenshots/tabos-%Y%m%d-%H%M%S.png", &local_time) == 0U) {
        SDL_Log("Could not format screenshot path");
        return false;
    }

    const int pitch      = (int) (TABOS_DISPLAY_WIDTH * sizeof(*presented_pixels));
    SDL_Surface* surface = SDL_CreateSurfaceFrom(TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT, SDL_PIXELFORMAT_RGB565,
                                                 presented_pixels, pitch);
    if (surface == NULL) {
        SDL_Log("Could not create screenshot surface: %s", SDL_GetError());
        return false;
    }
    const bool saved = SDL_SavePNG(surface, path);
    SDL_DestroySurface(surface);
    if (!saved) {
        SDL_Log("Could not save screenshot: %s", SDL_GetError());
        return false;
    }
    SDL_Log("Screenshot saved: %s", path);
    return true;
}

const char* platform_display_name(void)
{
    return "SDL3 RGB565";
}

uint32_t platform_graphics_capabilities(void)
{
    return TABOS_GRAPHICS_CAP_HARDWARE_ACCELERATED;
}

bool platform_graphics_begin(void)
{
    graphics_present_deadline_ns = SDL_GetTicksNS();
    return true;
}

void platform_graphics_end(void)
{
    graphics_present_deadline_ns = 0U;
}

bool platform_graphics_present(platform_framebuffer_t* framebuffer)
{
    if (!host_is_headless() && !renderer_vsync) {
        const Uint64 period_ns        = UINT64_C(1000000000) / TABOS_HOST_REFRESH_RATE_HZ;
        graphics_present_deadline_ns += period_ns;
        const Uint64 now              = SDL_GetTicksNS();
        if (now < graphics_present_deadline_ns) {
            SDL_DelayPrecise(graphics_present_deadline_ns - now);
        } else if (now - graphics_present_deadline_ns > period_ns) {
            graphics_present_deadline_ns = now;
        }
    }
    return platform_display_present(framebuffer);
}

bool platform_graphics_fill(platform_framebuffer_t* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height,
                            platform_pixel_t color)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || width == 0U || height == 0U ||
        framebuffer->width > INT_MAX || framebuffer->height > INT_MAX ||
        framebuffer->stride_pixels > INT_MAX / sizeof(*framebuffer->pixels)) {
        return false;
    }
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom((int) framebuffer->width, (int) framebuffer->height, SDL_PIXELFORMAT_RGB565,
                              framebuffer->pixels, (int) (framebuffer->stride_pixels * sizeof(*framebuffer->pixels)));
    if (surface == NULL) {
        return false;
    }
    const SDL_Rect rectangle = {.x = x, .y = y, .w = (int) width, .h = (int) height};
    const bool filled        = SDL_FillSurfaceRect(surface, &rectangle, color);
    SDL_DestroySurface(surface);
    return filled;
}

bool platform_graphics_blit(platform_framebuffer_t* framebuffer, const tabos_graphics_blit_options_t* options)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || options == NULL || options->pixels == NULL ||
        options->bitmap_width == 0U || options->bitmap_height == 0U || options->source.x < 0 || options->source.y < 0 ||
        options->source.width == 0U || options->source.height == 0U || options->destination.width == 0U ||
        options->destination.height == 0U || options->rotation != TABOS_GRAPHICS_ROTATE_0 || options->mirror_x ||
        options->mirror_y || options->opacity != 255U || options->color_key_enabled ||
        (uint64_t) (uint32_t) options->source.x + options->source.width > options->bitmap_width ||
        (uint64_t) (uint32_t) options->source.y + options->source.height > options->bitmap_height ||
        framebuffer->width > INT_MAX || framebuffer->height > INT_MAX ||
        framebuffer->stride_pixels > INT_MAX / sizeof(*framebuffer->pixels) || options->bitmap_width > INT_MAX ||
        options->bitmap_height > INT_MAX || options->bitmap_width > INT_MAX / sizeof(*options->pixels) ||
        options->source.width > INT_MAX || options->source.height > INT_MAX || options->destination.width > INT_MAX ||
        options->destination.height > INT_MAX) {
        return false;
    }

    SDL_Surface* source =
        SDL_CreateSurfaceFrom((int) options->bitmap_width, (int) options->bitmap_height, SDL_PIXELFORMAT_RGB565,
                              (void*) options->pixels, (int) (options->bitmap_width * sizeof(*options->pixels)));
    SDL_Surface* destination =
        SDL_CreateSurfaceFrom((int) framebuffer->width, (int) framebuffer->height, SDL_PIXELFORMAT_RGB565,
                              framebuffer->pixels, (int) (framebuffer->stride_pixels * sizeof(*framebuffer->pixels)));
    if (source == NULL || destination == NULL) {
        SDL_DestroySurface(source);
        SDL_DestroySurface(destination);
        return false;
    }
    const SDL_Rect source_rectangle = {
        .x = options->source.x,
        .y = options->source.y,
        .w = (int) options->source.width,
        .h = (int) options->source.height,
    };
    const SDL_Rect destination_rectangle = {
        .x = options->destination.x,
        .y = options->destination.y,
        .w = (int) options->destination.width,
        .h = (int) options->destination.height,
    };
    const bool blitted =
        SDL_BlitSurfaceScaled(source, &source_rectangle, destination, &destination_rectangle, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(source);
    SDL_DestroySurface(destination);
    return blitted;
}

bool platform_raster_fill_span(platform_pixel_t* destination, size_t count, platform_pixel_t color)
{
    (void) destination;
    (void) count;
    (void) color;
    return false;
}

bool platform_raster_copy_span(platform_pixel_t* destination, const platform_pixel_t* source, size_t count)
{
    (void) destination;
    (void) source;
    (void) count;
    return false;
}

void platform_raster_diagnostics(void)
{
}

bool platform_display_init(platform_framebuffer_t* framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }
    const size_t pixel_count = (size_t) TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    framebuffer_pixels       = calloc(pixel_count, sizeof(*framebuffer_pixels));
    if (framebuffer_pixels == NULL) {
        SDL_Log("Could not allocate host framebuffer");
        return false;
    }
    presented_pixels = calloc(pixel_count, sizeof(*presented_pixels));
    if (presented_pixels == NULL) {
        SDL_Log("Could not allocate host presented framebuffer");
        platform_display_shutdown();
        return false;
    }
    if (!host_is_headless()) {
        renderer = SDL_CreateRenderer(host_window, NULL);
        if (renderer == NULL) {
            SDL_Log("SDL renderer creation failed: %s", SDL_GetError());
            platform_display_shutdown();
            return false;
        }
        renderer_vsync = SDL_SetRenderVSync(renderer, 1);
        if (!renderer_vsync) {
            SDL_Log("Renderer VSYNC unavailable; using %u Hz timer pacing: %s", TABOS_HOST_REFRESH_RATE_HZ,
                    SDL_GetError());
        }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, TABOS_DISPLAY_WIDTH,
                                    TABOS_DISPLAY_HEIGHT);
        if (texture == NULL) {
            SDL_Log("SDL texture creation failed: %s", SDL_GetError());
            platform_display_shutdown();
            return false;
        }
        if (!SDL_SetRenderLogicalPresentation(renderer, TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT,
                                              SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            SDL_Log("SDL logical presentation setup failed: %s", SDL_GetError());
            platform_display_shutdown();
            return false;
        }
    }
    *framebuffer = (platform_framebuffer_t) {
        .pixels        = framebuffer_pixels,
        .width         = TABOS_DISPLAY_WIDTH,
        .height        = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool platform_display_present(const platform_framebuffer_t* framebuffer)
{
    if (framebuffer == NULL || framebuffer->pixels != framebuffer_pixels) {
        return false;
    }
    if (host_is_headless()) {
        return true;
    }
    const int pitch = (int) (framebuffer->stride_pixels * sizeof(*framebuffer->pixels));
    if (!SDL_UpdateTexture(texture, NULL, framebuffer->pixels, pitch)) {
        SDL_Log("SDL texture update failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE) || !SDL_RenderClear(renderer) ||
        !SDL_RenderTexture(renderer, texture, NULL, NULL) || !SDL_RenderPresent(renderer)) {
        SDL_Log("SDL framebuffer presentation failed: %s", SDL_GetError());
        return false;
    }
    memcpy(presented_pixels, framebuffer->pixels,
           (size_t) framebuffer->stride_pixels * framebuffer->height * sizeof(*framebuffer->pixels));
    return true;
}

void platform_display_shutdown(void)
{
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    renderer_vsync = false;
    free(framebuffer_pixels);
    framebuffer_pixels = NULL;
    free(presented_pixels);
    presented_pixels = NULL;
}
