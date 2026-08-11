#include "internal.h"

#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <stdlib.h>

static SDL_Renderer *renderer;
static SDL_Texture *texture;
static tab_pixel_t *framebuffer_pixels;

const char *tab_platform_display_name(void)
{
    return "SDL3 RGB565";
}

bool tab_platform_display_init(tab_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) return false;
    const size_t pixel_count = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    framebuffer_pixels = calloc(pixel_count, sizeof(*framebuffer_pixels));
    if (framebuffer_pixels == NULL) {
        SDL_Log("Could not allocate host framebuffer");
        return false;
    }
    if (!tab_host_is_headless()) {
        renderer = SDL_CreateRenderer(tab_host_window, NULL);
        if (renderer == NULL) {
            SDL_Log("SDL renderer creation failed: %s", SDL_GetError());
            tab_platform_display_shutdown();
            return false;
        }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING, TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT);
        if (texture == NULL) {
            SDL_Log("SDL texture creation failed: %s", SDL_GetError());
            tab_platform_display_shutdown();
            return false;
        }
        if (!SDL_SetRenderLogicalPresentation(renderer, TABOS_DISPLAY_WIDTH,
                TABOS_DISPLAY_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            SDL_Log("SDL logical presentation setup failed: %s", SDL_GetError());
            tab_platform_display_shutdown();
            return false;
        }
    }
    *framebuffer = (tab_framebuffer_t){
        .pixels = framebuffer_pixels,
        .width = TABOS_DISPLAY_WIDTH,
        .height = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool tab_platform_display_present(const tab_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL || framebuffer->pixels != framebuffer_pixels) return false;
    if (tab_host_is_headless()) return true;
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

void tab_platform_display_shutdown(void)
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
