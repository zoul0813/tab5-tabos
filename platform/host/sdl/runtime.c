#include "internal.h"

#include <tabos/platform/platform.h>

#include <tabos/battery.h>
#include <tabos/config/identity.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_Window* host_window;
static bool is_headless;
static bool quit_requested;
static platform_network_status_t network_status;
static bool host_battery_charging_enabled;
static bool host_battery_fast_charging_enabled;

static char* window_state_path(void)
{
    char* pref_path = SDL_GetPrefPath(TABOS_HOST_PREFERENCES_ORGANIZATION, TABOS_HOST_PREFERENCES_APPLICATION);
    if (pref_path == NULL) {
        return NULL;
    }
    static const char filename[] = TABOS_HOST_WINDOW_STATE_FILENAME;
    const size_t path_size       = strlen(pref_path) + sizeof(filename);
    char* path                   = malloc(path_size);
    if (path != NULL) {
        (void) snprintf(path, path_size, "%s%s", pref_path, filename);
    }
    SDL_free(pref_path);
    return path;
}

static bool position_is_visible(int x, int y)
{
    int display_count       = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&display_count);
    if (displays == NULL) {
        return false;
    }
    const long long window_left   = x;
    const long long window_top    = y;
    const long long window_right  = window_left + TABOS_DISPLAY_WIDTH;
    const long long window_bottom = window_top + TABOS_DISPLAY_HEIGHT;
    bool visible                  = false;
    for (int index = 0; index < display_count; ++index) {
        SDL_Rect bounds;
        if (!SDL_GetDisplayUsableBounds(displays[index], &bounds)) {
            continue;
        }
        const long long display_right  = (long long) bounds.x + bounds.w;
        const long long display_bottom = (long long) bounds.y + bounds.h;
        if (window_left < display_right && window_right > bounds.x && window_top < display_bottom &&
            window_bottom > bounds.y) {
            visible = true;
            break;
        }
    }
    SDL_free(displays);
    return visible;
}

static void restore_window_position(void)
{
    char* path = window_state_path();
    if (path == NULL) {
        return;
    }
    FILE* state = fopen(path, "r");
    free(path);
    if (state == NULL) {
        return;
    }
    int x                 = 0;
    int y                 = 0;
    const int values_read = fscanf(state, "%d %d", &x, &y);
    (void) fclose(state);
    if (values_read == 2 && position_is_visible(x, y) && !SDL_SetWindowPosition(host_window, x, y)) {
        SDL_Log("Could not restore window position: %s", SDL_GetError());
    }
}

static void save_window_position(void)
{
    int x = 0;
    int y = 0;
    if (host_window == NULL || !SDL_GetWindowPosition(host_window, &x, &y)) {
        return;
    }
    char* path = window_state_path();
    if (path == NULL) {
        return;
    }
    FILE* state = fopen(path, "w");
    free(path);
    if (state == NULL) {
        return;
    }
    (void) fprintf(state, "%d %d\n", x, y);
    (void) fclose(state);
}

bool host_is_headless(void)
{
    return is_headless;
}

void host_request_quit(void)
{
    quit_requested = true;
}

void platform_stop_run_loop(void)
{
    host_request_quit();
}

void platform_perform_system_action(platform_system_action_t action)
{
    (void) action;
}

bool platform_init(bool headless)
{
    is_headless                        = headless;
    quit_requested                     = false;
    host_battery_charging_enabled      = true;
    host_battery_fast_charging_enabled = false;
    const SDL_InitFlags flags          = headless ? 0U : SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO;
    if (!SDL_Init(flags)) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return false;
    }
    if (is_headless) {
        return true;
    }
    host_window = SDL_CreateWindow(TABOS_HOST_WINDOW_TITLE, TABOS_DISPLAY_WIDTH, TABOS_DISPLAY_HEIGHT,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (host_window == NULL) {
        SDL_Log("SDL window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }
    restore_window_position();
    if (!SDL_ShowWindow(host_window)) {
        SDL_Log("SDL window display failed: %s", SDL_GetError());
        SDL_DestroyWindow(host_window);
        host_window = NULL;
        SDL_Quit();
        return false;
    }
    if (!SDL_StartTextInput(host_window)) {
        SDL_Log("SDL text input initialization failed: %s", SDL_GetError());
        platform_shutdown();
        return false;
    }
    return true;
}

bool platform_network_init(const char* hostname)
{
    (void) hostname;
    network_status = (platform_network_status_t) {.state = PLATFORM_NETWORK_OFFLINE};
    return true;
}

void platform_network_shutdown(void)
{
    network_status = (platform_network_status_t) {0};
}

bool platform_network_connect(const char* ssid, const char* password)
{
    (void) password;
    if (ssid == NULL || ssid[0] == '\0') {
        return false;
    }
    network_status = (platform_network_status_t) {
        .state      = PLATFORM_NETWORK_ONLINE,
        .signal_dbm = -30,
    };
    (void) snprintf(network_status.ssid, sizeof(network_status.ssid), "%s", ssid);
    (void) snprintf(network_status.ipv4, sizeof(network_status.ipv4), "127.0.0.1");
    return true;
}

bool platform_network_disconnect(void)
{
    network_status.state   = PLATFORM_NETWORK_OFFLINE;
    network_status.ipv4[0] = '\0';
    return true;
}

bool platform_network_status(platform_network_status_t* status)
{
    if (status == NULL) {
        return false;
    }
    *status = network_status;
    return true;
}

bool platform_battery_status(platform_battery_status_t* status)
{
    if (status == NULL) {
        return false;
    }
    *status = (platform_battery_status_t) {
        .available              = true,
        .external_power_present = true,
        .charging_enabled       = host_battery_charging_enabled,
        .fast_charging_enabled  = host_battery_fast_charging_enabled,
        .valid = TABOS_BATTERY_VALID_STATE | TABOS_BATTERY_VALID_SOURCE | TABOS_BATTERY_VALID_PERCENTAGE |
                 TABOS_BATTERY_VALID_VOLTAGE | TABOS_BATTERY_VALID_CURRENT | TABOS_BATTERY_VALID_POWER |
                 TABOS_BATTERY_VALID_CHARGING_CONTROL | TABOS_BATTERY_VALID_FAST_CHARGING_CONTROL,
        .voltage_mv   = 8000U,
        .current_ma   = -500,
        .power_mw     = -4000,
        .percentage   = 80U,
        .charge_state = TABOS_BATTERY_STATE_CHARGING,
    };
    return true;
}

bool platform_battery_set_charging(bool enabled)
{
    host_battery_charging_enabled = enabled;
    return true;
}

bool platform_battery_set_fast_charging(bool enabled)
{
    host_battery_fast_charging_enabled = enabled;
    return true;
}

bool platform_battery_health(int* error)
{
    if (error != NULL) {
        *error = 0;
    }
    return true;
}

int platform_run(platform_update_fn update)
{
    if (is_headless) {
        if (update != NULL) {
            update();
        }
        return 0;
    }
    while (!quit_requested) {
        /* Poll input without letting event arrival control emulation speed. */
        host_input_update(false);
        if (update != NULL) {
            update();
        }
        SDL_Delay(1);
    }
    return 0;
}

void platform_shutdown(void)
{
    platform_display_shutdown();
    if (host_window != NULL) {
        SDL_StopTextInput(host_window);
        save_window_position();
        SDL_DestroyWindow(host_window);
        host_window = NULL;
    }
    SDL_Quit();
}

const char* platform_name(void)
{
#if defined(TABOS_HOST_MACOS)
    return TABOS_TARGET_NAME_MACOS;
#elif defined(TABOS_HOST_LINUX)
    return TABOS_TARGET_NAME_LINUX;
#else
    return "host";
#endif
}

bool platform_get_diagnostics(platform_diagnostics_t* diagnostics)
{
    if (diagnostics == NULL) {
        return false;
    }
    const int ram_mebibytes = SDL_GetSystemRAM();
    const int cpu_cores     = SDL_GetNumLogicalCPUCores();
    *diagnostics            = (platform_diagnostics_t) {
                   .device_name        = "Native host",
                   .cpu_cores          = cpu_cores > 0 ? (unsigned int) cpu_cores : 0U,
                   .memory_total_bytes = ram_mebibytes > 0 ? (uint64_t) (unsigned int) ram_mebibytes * 1024U * 1024U : 0U,
                   .keyboard_name      = "SDL3 keyboard",
                   .keyboard_driver    = "SDL3",
                   .keyboard_present   = true,
                   .keyboard_detected  = true,
                   .rtc_name           = "host-clock",
                   .rtc_present        = true,
                   .rtc_detected       = true,
                   .battery_name       = "host-power",
                   .battery_present    = true,
                   .battery_detected   = true,
                   .network_name       = "host-network",
                   .network_present    = true,
    };
    return true;
}

void platform_log(const char* message)
{
    if (message != NULL) {
        (void) printf("%s\n", message);
    }
}

uint64_t platform_time_ms(void)
{
    return SDL_GetTicks();
}
