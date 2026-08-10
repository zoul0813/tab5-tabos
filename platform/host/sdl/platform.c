#include <tabos/platform/platform.h>

#include <tabos/config/identity.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static tab_pixel_t *framebuffer_pixels;
static bool is_headless;

static char *window_state_path(void)
{
    char *pref_path = SDL_GetPrefPath(
        TABOS_HOST_PREFERENCES_ORGANIZATION,
        TABOS_HOST_PREFERENCES_APPLICATION
    );
    if (pref_path == NULL) {
        return NULL;
    }

    static const char filename[] = TABOS_HOST_WINDOW_STATE_FILENAME;
    const size_t path_size = strlen(pref_path) + sizeof(filename);
    char *path = malloc(path_size);

    if (path != NULL) {
        (void)snprintf(path, path_size, "%s%s", pref_path, filename);
    }

    SDL_free(pref_path);
    return path;
}

static bool position_is_visible(int x, int y)
{
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);

    if (displays == NULL) {
        return false;
    }

    const long long window_left = x;
    const long long window_top = y;
    const long long window_right = window_left + TABOS_DISPLAY_WIDTH;
    const long long window_bottom = window_top + TABOS_DISPLAY_HEIGHT;
    bool visible = false;

    for (int index = 0; index < display_count; ++index) {
        SDL_Rect bounds;
        if (!SDL_GetDisplayUsableBounds(displays[index], &bounds)) {
            continue;
        }

        const long long display_left = bounds.x;
        const long long display_top = bounds.y;
        const long long display_right = display_left + bounds.w;
        const long long display_bottom = display_top + bounds.h;

        if (window_left < display_right && window_right > display_left &&
            window_top < display_bottom && window_bottom > display_top) {
            visible = true;
            break;
        }
    }

    SDL_free(displays);
    return visible;
}

static void restore_window_position(void)
{
    char *path = window_state_path();
    if (path == NULL) {
        return;
    }

    FILE *state = fopen(path, "r");
    free(path);
    if (state == NULL) {
        return;
    }

    int x = 0;
    int y = 0;
    const int values_read = fscanf(state, "%d %d", &x, &y);
    (void)fclose(state);

    if (values_read == 2 && position_is_visible(x, y) &&
        !SDL_SetWindowPosition(window, x, y)) {
        SDL_Log("Could not restore window position: %s", SDL_GetError());
    }
}

static void save_window_position(void)
{
    int x = 0;
    int y = 0;

    if (window == NULL || !SDL_GetWindowPosition(window, &x, &y)) {
        return;
    }

    char *path = window_state_path();
    if (path == NULL) {
        return;
    }

    FILE *state = fopen(path, "w");
    free(path);
    if (state == NULL) {
        return;
    }

    (void)fprintf(state, "%d %d\n", x, y);
    (void)fclose(state);
}

bool tab_platform_init(bool headless)
{
    is_headless = headless;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return false;
    }

    if (is_headless) {
        return true;
    }

    window = SDL_CreateWindow(
        TABOS_HOST_WINDOW_TITLE,
        TABOS_DISPLAY_WIDTH,
        TABOS_DISPLAY_HEIGHT,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
    );
    if (window == NULL) {
        SDL_Log("SDL window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    restore_window_position();
    if (!SDL_ShowWindow(window)) {
        SDL_Log("SDL window display failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        SDL_Quit();
        return false;
    }

    return true;
}

int tab_platform_run(void)
{
    SDL_Event event;

    if (is_headless) {
        return 0;
    }

    for (;;) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return 0;
            }
        }

        SDL_Delay(10);
    }
}

void tab_platform_shutdown(void)
{
    tab_platform_display_shutdown();

    if (window != NULL) {
        save_window_position();
        SDL_DestroyWindow(window);
        window = NULL;
    }

    SDL_Quit();
}

const char *tab_platform_name(void)
{
#if defined(TABOS_HOST_MACOS)
    return TABOS_TARGET_NAME_MACOS;
#elif defined(TABOS_HOST_LINUX)
    return TABOS_TARGET_NAME_LINUX;
#else
    return "host";
#endif
}

const char *tab_platform_display_name(void)
{
    return "SDL3 RGB565";
}

bool tab_platform_display_init(tab_framebuffer_t *framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }

    const size_t pixel_count = (size_t)TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT;
    framebuffer_pixels = calloc(pixel_count, sizeof(*framebuffer_pixels));
    if (framebuffer_pixels == NULL) {
        SDL_Log("Could not allocate host framebuffer");
        return false;
    }

    if (!is_headless) {
        renderer = SDL_CreateRenderer(window, NULL);
        if (renderer == NULL) {
            SDL_Log("SDL renderer creation failed: %s", SDL_GetError());
            tab_platform_display_shutdown();
            return false;
        }

        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            TABOS_DISPLAY_WIDTH,
            TABOS_DISPLAY_HEIGHT
        );
        if (texture == NULL) {
            SDL_Log("SDL texture creation failed: %s", SDL_GetError());
            tab_platform_display_shutdown();
            return false;
        }

        if (!SDL_SetRenderLogicalPresentation(
                renderer,
                TABOS_DISPLAY_WIDTH,
                TABOS_DISPLAY_HEIGHT,
                SDL_LOGICAL_PRESENTATION_LETTERBOX
            )) {
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
    if (framebuffer == NULL || framebuffer->pixels != framebuffer_pixels) {
        return false;
    }

    if (is_headless) {
        return true;
    }

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
