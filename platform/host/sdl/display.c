#include "internal.h"

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static SDL_Renderer *renderer;
static SDL_Texture *texture;
static platform_pixel_t *framebuffer_pixels;

bool host_capture_screenshot(void)
{
    if (framebuffer_pixels == NULL || host_is_headless()) return false;
    if (!SDL_CreateDirectory("screenshots")) {
        SDL_Log("Could not create screenshot directory: %s", SDL_GetError());
        return false;
    }

    const time_t now = time(NULL);
    struct tm local_time;
    if (now == (time_t)-1 || localtime_r(&now, &local_time) == NULL) {
        SDL_Log("Could not create screenshot timestamp");
        return false;
    }
    char path[80];
    if (strftime(path, sizeof(path), "screenshots/tabos-%Y%m%d-%H%M%S.bmp", &local_time) == 0U) {
        SDL_Log("Could not format screenshot path");
        return false;
    }

    const int pitch = (int)(TABOS_DISPLAY_WIDTH * sizeof(*framebuffer_pixels));
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT, SDL_PIXELFORMAT_RGB565,
        framebuffer_pixels, pitch);
    if (surface == NULL) {
        SDL_Log("Could not create screenshot surface: %s", SDL_GetError());
        return false;
    }
    const bool saved = SDL_SaveBMP(surface, path);
    SDL_DestroySurface(surface);
    if (!saved) {
        SDL_Log("Could not save screenshot: %s", SDL_GetError());
        return false;
    }
    SDL_Log("Screenshot saved: %s", path);
    return true;
}

const char *platform_display_name(void)
{
    return "SDL3 RGB565";
}

bool platform_display_init(platform_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) return false;
    const size_t pixel_count = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    framebuffer_pixels = calloc(pixel_count, sizeof(*framebuffer_pixels));
    if (framebuffer_pixels == NULL) {
        SDL_Log("Could not allocate host framebuffer");
        return false;
    }
    if (!host_is_headless()) {
        renderer = SDL_CreateRenderer(host_window, NULL);
        if (renderer == NULL) {
            SDL_Log("SDL renderer creation failed: %s", SDL_GetError());
            platform_display_shutdown();
            return false;
        }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING, TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT);
        if (texture == NULL) {
            SDL_Log("SDL texture creation failed: %s", SDL_GetError());
            platform_display_shutdown();
            return false;
        }
        if (!SDL_SetRenderLogicalPresentation(renderer, TABOS_DISPLAY_WIDTH,
                TABOS_DISPLAY_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            SDL_Log("SDL logical presentation setup failed: %s", SDL_GetError());
            platform_display_shutdown();
            return false;
        }
    }
    *framebuffer = (platform_framebuffer_t){
        .pixels = framebuffer_pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool platform_display_present(const platform_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL || framebuffer->pixels != framebuffer_pixels) return false;
    if (host_is_headless()) return true;
    const int pitch = (int)(framebuffer->stride_pixels * sizeof(*framebuffer->pixels));
    if (!SDL_UpdateTexture(texture, NULL, framebuffer->pixels, pitch)) {
        SDL_Log("SDL texture update failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE) ||
        !SDL_RenderClear(renderer) || !SDL_RenderTexture(renderer, texture, NULL, NULL) ||
        !SDL_RenderPresent(renderer)) {
        SDL_Log("SDL framebuffer presentation failed: %s", SDL_GetError());
        return false;
    }
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
    free(framebuffer_pixels);
    framebuffer_pixels = NULL;
}
