#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>

#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static platform_pixel_t pixels[TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT];
static platform_pixel_t direct_pixels[TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT];
static platform_pixel_t direct_overlay_saved[4096];
static bool fake_direct_graphics;
static bool fake_direct_overlay_active;
static int32_t direct_overlay_x;
static int32_t direct_overlay_y;
static uint32_t direct_overlay_width;
static uint32_t direct_overlay_height;
static size_t comparison_x;
static size_t comparison_y;
static size_t comparison_width;
static size_t comparison_height;
static bool comparison_matches;

uint32_t platform_graphics_capabilities(void)
{
    return 0U;
}

bool platform_graphics_begin(void)
{
    return true;
}

void platform_graphics_end(void)
{
}

bool platform_graphics_overlay(platform_framebuffer_t* framebuffer, const platform_graphics_overlay_t* overlay)
{
    if (framebuffer == NULL) {
        return false;
    }
    if (!fake_direct_graphics) {
        return true;
    }
    if (overlay == NULL) {
        if (fake_direct_overlay_active) {
            for (uint32_t row = 0U; row < direct_overlay_height; ++row) {
                for (uint32_t column = 0U; column < direct_overlay_width; ++column) {
                    const size_t destination =
                        (size_t) (direct_overlay_y + (int32_t) row) * framebuffer->stride_pixels +
                        (size_t) (direct_overlay_x + (int32_t) column);
                    direct_pixels[destination] = direct_overlay_saved[(size_t) row * direct_overlay_width + column];
                }
            }
            fake_direct_overlay_active = false;
        }
        return true;
    }
    if ((uint64_t) overlay->width * overlay->height > 4096U) {
        return false;
    }
    direct_overlay_x      = overlay->x;
    direct_overlay_y      = overlay->y;
    direct_overlay_width  = overlay->width;
    direct_overlay_height = overlay->height;
    for (uint32_t row = 0U; row < overlay->height; ++row) {
        for (uint32_t column = 0U; column < overlay->width; ++column) {
            const size_t source = (size_t) (overlay->y + (int32_t) row) * framebuffer->stride_pixels +
                                  (size_t) (overlay->x + (int32_t) column);
            const size_t overlay_index          = (size_t) row * overlay->width + column;
            direct_overlay_saved[overlay_index] = direct_pixels[source];
            if (framebuffer->pixels[source] != overlay->background[overlay_index]) {
                direct_pixels[source] = framebuffer->pixels[source];
            }
        }
    }
    fake_direct_overlay_active = true;
    return true;
}

bool platform_graphics_present(platform_framebuffer_t* framebuffer)
{
    return fake_direct_graphics ? framebuffer != NULL : platform_display_present(framebuffer);
}

void test_platform_graphics_direct_begin(platform_pixel_t color)
{
    for (size_t index = 0U; index < TABOS_DISPLAY_WIDTH * TABOS_DISPLAY_HEIGHT; ++index) {
        direct_pixels[index] = color;
    }
    fake_direct_graphics       = true;
    fake_direct_overlay_active = false;
}

void test_platform_graphics_direct_end(void)
{
    fake_direct_graphics = false;
}

void test_platform_graphics_direct_resume(void)
{
    fake_direct_graphics = true;
}

platform_pixel_t test_platform_graphics_pixel(size_t x, size_t y)
{
    return direct_pixels[y * TABOS_DISPLAY_WIDTH + x];
}

bool platform_graphics_fill(platform_framebuffer_t* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height,
                            platform_pixel_t color)
{
    (void) framebuffer;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) color;
    return false;
}

bool platform_graphics_blit(platform_framebuffer_t* framebuffer, const tabos_graphics_blit_options_t* options)
{
    (void) framebuffer;
    (void) options;
    return false;
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

struct platform_mutex {
        atomic_flag locked;
};

static uint64_t monotonic_ms;
static uint8_t fake_brightness = 100U;
static bool fail_brightness_once;
static bool fake_panel_enabled = true;
static bool fail_panel_once;
static unsigned int panel_calls;
static char last_log[256];
static platform_network_status_t fake_network;
static platform_network_event_fn fake_network_event;
static unsigned int network_connect_calls;
static unsigned int network_status_calls;
static char network_hostname[33];
static bool fake_rtc_ready = true;
static int fake_rtc_error;
static bool fake_keyboard_ready = true;
static int fake_keyboard_error;
static unsigned int keyboard_update_calls;
static unsigned int pointer_update_calls;
static bool fake_battery_ready = true;
static int fake_battery_error;
static platform_audio_error_fn fake_audio_error;
static platform_audio_render_fn fake_audio_render;
static platform_audio_capture_fn fake_audio_capture;
static uint32_t fake_audio_sample_rate;
static bool fake_audio_active;
static bool fake_audio_fail_start;
static unsigned int fake_audio_start_calls;
static unsigned int fake_audio_stop_calls;
static uint32_t fake_audio_route;
static unsigned int fake_audio_route_calls;
static platform_runtime_events_t fake_runtime_events;
static uint64_t fake_runtime_wait_deadline;
static unsigned int fake_display_present_calls;

bool platform_display_init(platform_framebuffer_t* framebuffer)
{
    if (framebuffer == NULL) {
        return false;
    }

    *framebuffer = (platform_framebuffer_t) {
        .pixels        = pixels,
        .width         = TABOS_DISPLAY_WIDTH,
        .height        = TABOS_DISPLAY_HEIGHT,
        .stride_pixels = TABOS_DISPLAY_WIDTH,
    };
    return true;
}

bool platform_display_present(const platform_framebuffer_t* framebuffer)
{
    ++fake_display_present_calls;
    if (framebuffer == NULL || framebuffer->pixels != pixels || comparison_x >= framebuffer->width ||
        comparison_y >= framebuffer->height || comparison_width > framebuffer->width - comparison_x ||
        comparison_height > framebuffer->height - comparison_y) {
        return false;
    }
    comparison_matches = true;
    for (size_t row = 0U; row < comparison_height; ++row) {
        for (size_t column = 0U; column < comparison_width; ++column) {
            const size_t index = (comparison_y + row) * framebuffer->stride_pixels + comparison_x + column;
            if (framebuffer->pixels[index] != direct_pixels[index]) {
                comparison_matches = false;
            }
        }
    }
    return true;
}

void test_platform_display_compare_graphics_region(size_t x, size_t y, size_t width, size_t height)
{
    comparison_x      = x;
    comparison_y      = y;
    comparison_width  = width;
    comparison_height = height;
}

bool test_platform_display_graphics_region_matches(void)
{
    return comparison_matches;
}

unsigned int test_platform_display_present_calls(void)
{
    return fake_display_present_calls;
}

void platform_display_shutdown(void)
{
}

void platform_stop_run_loop(void)
{
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_SHUTDOWN);
}

void platform_runtime_notify(platform_runtime_events_t events)
{
    fake_runtime_events |= events;
}

void platform_runtime_notify_from_isr(platform_runtime_events_t events)
{
    platform_runtime_notify(events);
}

platform_runtime_events_t platform_runtime_wait_until(uint64_t deadline_ms)
{
    fake_runtime_wait_deadline       = deadline_ms;
    platform_runtime_events_t events = fake_runtime_events;
    fake_runtime_events              = PLATFORM_RUNTIME_EVENT_NONE;
    if (deadline_ms != PLATFORM_RUNTIME_DEADLINE_NONE && monotonic_ms >= deadline_ms) {
        events |= PLATFORM_RUNTIME_EVENT_DEADLINE;
    }
    return events;
}

void platform_perform_system_action(platform_system_action_t action)
{
    (void) action;
}

const char* platform_name(void)
{
    return "test";
}

const char* platform_display_name(void)
{
    return "test display";
}

bool platform_get_diagnostics(platform_diagnostics_t* diagnostics)
{
    if (diagnostics == NULL) {
        return false;
    }
    *diagnostics = (platform_diagnostics_t) {
        .device_name        = "TEST DEVICE",
        .cpu_cores          = 2U,
        .cpu_frequency_mhz  = 100U,
        .memory_total_bytes = 1024U,
        .memory_free_bytes  = 512U,
        .memory_free_known  = true,
        .keyboard_name      = "TEST KEYBOARD",
        .keyboard_driver    = "TEST KEYBOARD",
        .keyboard_present   = fake_keyboard_ready,
        .keyboard_detected  = true,
        .keyboard_error     = fake_keyboard_error,
        .rtc_name           = "TEST RTC",
        .rtc_present        = fake_rtc_ready,
        .rtc_detected       = true,
        .rtc_error          = fake_rtc_error,
        .battery_name       = "TEST BATTERY",
        .battery_present    = fake_battery_ready,
        .battery_detected   = true,
        .battery_error      = fake_battery_error,
        .network_name       = "TEST NETWORK",
        .network_present    = true,
    };
    return true;
}

void platform_log(const char* message)
{
    (void) snprintf(last_log, sizeof(last_log), "%s", message != NULL ? message : "");
}

const char* test_platform_last_log(void)
{
    return last_log;
}

void test_platform_clear_log(void)
{
    last_log[0] = '\0';
}

uint64_t platform_time_ms(void)
{
    return monotonic_ms;
}

bool platform_power_set_brightness(uint8_t percent)
{
    if (percent > 100U || fail_brightness_once || (percent != 0U && !fake_panel_enabled)) {
        fail_brightness_once = false;
        return false;
    }
    fake_brightness = percent;
    return true;
}
uint8_t test_platform_brightness(void)
{
    return fake_brightness;
}
void test_platform_fail_brightness_once(void)
{
    fail_brightness_once = true;
}
bool platform_power_set_panel_enabled(bool enabled)
{
    ++panel_calls;
    if (fail_panel_once) {
        fail_panel_once = false;
        return false;
    }
    if (!enabled && fake_brightness != 0U) {
        return false;
    }
    fake_panel_enabled = enabled;
    return true;
}
bool test_platform_panel_enabled(void)
{
    return fake_panel_enabled;
}
unsigned int test_platform_panel_calls(void)
{
    return panel_calls;
}
void test_platform_fail_panel_once(void)
{
    fail_panel_once = true;
}
bool platform_power_prepare_sleep(void)
{
    return true;
}
void platform_power_abort_sleep(void)
{
}
bool platform_power_enter_light_sleep(void)
{
    return true;
}
platform_power_wake_cause_t platform_power_collect_wake_causes(void)
{
    return PLATFORM_POWER_WAKE_KEYBOARD;
}
bool platform_power_restore(void)
{
    return true;
}

bool platform_wall_clock_get(int64_t* seconds)
{
    if (seconds == NULL) {
        return false;
    }
    *seconds = 1704067200;
    return true;
}

bool platform_wall_clock_set(int64_t seconds)
{
    return seconds >= 0;
}

bool platform_wall_clock_status(int* error)
{
    if (error != NULL) {
        *error = fake_rtc_error;
    }
    return fake_rtc_ready && fake_rtc_error == 0;
}

bool platform_keyboard_health(int* error)
{
    if (error != NULL) {
        *error = fake_keyboard_error;
    }
    return fake_keyboard_ready && fake_keyboard_error == 0;
}

void platform_keyboard_update(void)
{
    ++keyboard_update_calls;
}

unsigned int test_platform_keyboard_update_calls(void)
{
    return keyboard_update_calls;
}

void test_platform_keyboard_set_status(bool ready, int error)
{
    fake_keyboard_ready = ready;
    fake_keyboard_error = error;
}

void test_platform_rtc_set_status(bool ready, int error)
{
    fake_rtc_ready = ready;
    fake_rtc_error = error;
}

bool platform_battery_health(int* error)
{
    if (error != NULL) {
        *error = fake_battery_error;
    }
    return fake_battery_ready && fake_battery_error == 0;
}

void test_platform_battery_set_status(bool ready, int error)
{
    fake_battery_ready = ready;
    fake_battery_error = error;
}

bool platform_audio_init(platform_audio_render_fn render, platform_audio_capture_fn capture,
                         platform_audio_error_fn error, platform_audio_info_t* info)
{
    if (render == NULL || capture == NULL || error == NULL || info == NULL) {
        return false;
    }
    fake_audio_render  = render;
    fake_audio_capture = capture;
    fake_audio_error   = error;
    *info              = (platform_audio_info_t) {
                     .driver              = "fake-audio",
                     .features            = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
                     .routes              = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE | TABOS_AUDIO_ROUTE_MICROPHONE,
                     .capture_channels    = 4U,
                     .sample_rates        = TABOS_AUDIO_RATES_ALL,
                     .default_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE,
                     .detected            = true,
                     .ready               = true,
    };
    fake_audio_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE;
    fake_audio_active      = false;
    fake_audio_fail_start  = false;
    fake_audio_start_calls = 0U;
    fake_audio_stop_calls  = 0U;
    fake_audio_route       = 0U;
    fake_audio_route_calls = 0U;
    return true;
}

void platform_audio_shutdown(void)
{
    fake_audio_render  = NULL;
    fake_audio_capture = NULL;
    fake_audio_error   = NULL;
    fake_audio_active  = false;
}

bool platform_audio_set_route(uint32_t route)
{
    if (route != TABOS_AUDIO_ROUTE_SPEAKER && route != TABOS_AUDIO_ROUTE_HEADPHONE &&
        route != TABOS_AUDIO_ROUTE_MICROPHONE) {
        return false;
    }
    fake_audio_route = route;
    ++fake_audio_route_calls;
    return true;
}

bool platform_audio_start(uint32_t sample_rate, uint32_t route)
{
    ++fake_audio_start_calls;
    if (fake_audio_fail_start) {
        fake_audio_fail_start = false;
        return false;
    }
    if (route != TABOS_AUDIO_ROUTE_SPEAKER && route != TABOS_AUDIO_ROUTE_HEADPHONE &&
        route != TABOS_AUDIO_ROUTE_MICROPHONE) {
        return false;
    }
    switch (sample_rate) {
        case TABOS_AUDIO_SAMPLE_RATE_8000:
        case TABOS_AUDIO_SAMPLE_RATE_11025:
        case TABOS_AUDIO_SAMPLE_RATE_12000:
        case TABOS_AUDIO_SAMPLE_RATE_16000:
        case TABOS_AUDIO_SAMPLE_RATE_22050:
        case TABOS_AUDIO_SAMPLE_RATE_24000:
        case TABOS_AUDIO_SAMPLE_RATE_32000:
        case TABOS_AUDIO_SAMPLE_RATE_44100:
        case TABOS_AUDIO_SAMPLE_RATE_48000:
        case TABOS_AUDIO_SAMPLE_RATE_88200:
        case TABOS_AUDIO_SAMPLE_RATE_96000:
            fake_audio_sample_rate = sample_rate;
            fake_audio_active      = true;
            fake_audio_route       = route;
            return true;
        default: return false;
    }
}

void platform_audio_stop(void)
{
    if (fake_audio_active) {
        ++fake_audio_stop_calls;
    }
    fake_audio_active = false;
}

uint32_t test_platform_audio_sample_rate(void)
{
    return fake_audio_sample_rate;
}

bool test_platform_audio_active(void)
{
    return fake_audio_active;
}

unsigned int test_platform_audio_start_calls(void)
{
    return fake_audio_start_calls;
}

unsigned int test_platform_audio_stop_calls(void)
{
    return fake_audio_stop_calls;
}

void test_platform_audio_fail_start_once(void)
{
    fake_audio_fail_start = true;
}

uint32_t test_platform_audio_route(void)
{
    return fake_audio_route;
}

unsigned int test_platform_audio_route_calls(void)
{
    return fake_audio_route_calls;
}

void test_platform_audio_render(int16_t* stereo, size_t frames)
{
    if (fake_audio_render != NULL) {
        fake_audio_render(stereo, frames);
    }
}

void test_platform_audio_capture(const int16_t* samples, size_t frames, uint32_t channels)
{
    if (fake_audio_capture != NULL) {
        fake_audio_capture(samples, frames, channels);
    }
}

void test_platform_audio_error(int error)
{
    if (fake_audio_error != NULL) {
        fake_audio_error(error);
    }
}

bool platform_network_init(const char* hostname, platform_network_event_fn event)
{
    fake_network_event    = event;
    fake_network          = (platform_network_status_t) {.state = PLATFORM_NETWORK_OFFLINE};
    network_connect_calls = 0U;
    network_status_calls  = 0U;
    (void) snprintf(network_hostname, sizeof(network_hostname), "%s", hostname != NULL ? hostname : "");
    if (fake_network_event != NULL) {
        fake_network_event();
    }
    return true;
}

void platform_network_shutdown(void)
{
    fake_network       = (platform_network_status_t) {0};
    fake_network_event = NULL;
}

bool platform_network_connect(const char* ssid, const char* password)
{
    (void) password;
    if (ssid == NULL || ssid[0] == '\0') {
        return false;
    }
    ++network_connect_calls;
    fake_network.state = PLATFORM_NETWORK_CONNECTING;
    (void) snprintf(fake_network.ssid, sizeof(fake_network.ssid), "%s", ssid);
    if (fake_network_event != NULL) {
        fake_network_event();
    }
    return true;
}

bool platform_network_disconnect(void)
{
    fake_network.state = PLATFORM_NETWORK_OFFLINE;
    if (fake_network_event != NULL) {
        fake_network_event();
    }
    return true;
}

bool platform_network_status(platform_network_status_t* status)
{
    if (status == NULL) {
        return false;
    }
    ++network_status_calls;
    *status = fake_network;
    return true;
}

bool platform_network_operations_init(void)
{
    return true;
}

void platform_network_operations_shutdown(void)
{
}

bool platform_network_socket_operations_init(void)
{
    return true;
}

void platform_network_socket_operations_shutdown(void)
{
}

void platform_network_socket_interrupt(int socket)
{
    (void) socket;
}

bool platform_network_socket_operations_suspend(void)
{
    return true;
}

void platform_network_socket_operations_resume(void)
{
}

void platform_network_socket_dispose(int socket)
{
    (void) socket;
}

int platform_network_socket_open(uint32_t family, uint32_t type)
{
    (void) family;
    (void) type;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_close(int socket)
{
    (void) socket;
    return 0;
}

#define TEST_SOCKET_ENDPOINT_OPERATION(name)                                       \
    int name(int socket, const platform_network_address_t* address, uint16_t port) \
    {                                                                              \
        (void) socket;                                                             \
        (void) address;                                                            \
        (void) port;                                                               \
        return -TABOS_ENOTSUP;                                                     \
    }

TEST_SOCKET_ENDPOINT_OPERATION(platform_network_socket_bind)
TEST_SOCKET_ENDPOINT_OPERATION(platform_network_socket_connect)

int platform_network_socket_get_local_endpoint(int socket, platform_network_address_t* address, uint16_t* port)
{
    (void) socket;
    (void) address;
    (void) port;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_listen(int socket, uint16_t backlog)
{
    (void) socket;
    (void) backlog;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_accept(int socket, platform_network_address_t* address, uint16_t* port)
{
    (void) socket;
    (void) address;
    (void) port;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_set_nonblocking(int socket, bool enabled)
{
    (void) socket;
    (void) enabled;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_shutdown(int socket, uint32_t direction)
{
    (void) socket;
    (void) direction;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_send(int socket, const void* data, uint32_t size)
{
    (void) socket;
    (void) data;
    (void) size;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_receive(int socket, void* data, uint32_t capacity)
{
    (void) socket;
    (void) data;
    (void) capacity;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_send_to(int socket, const void* data, uint32_t size,
                                    const platform_network_address_t* address, uint16_t port)
{
    (void) socket;
    (void) data;
    (void) size;
    (void) address;
    (void) port;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_receive_from(int socket, void* data, uint32_t capacity, platform_network_address_t* address,
                                         uint16_t* port)
{
    (void) socket;
    (void) data;
    (void) capacity;
    (void) address;
    (void) port;
    return -TABOS_ENOTSUP;
}

int platform_network_socket_wait(platform_network_wait_item_t* items, uint32_t count, uint32_t timeout_ms)
{
    (void) items;
    (void) count;
    (void) timeout_ms;
    return -TABOS_ENOTSUP;
}

int platform_tls_connect(const char* hostname, uint16_t port)
{
    (void) hostname;
    (void) port;
    return -TABOS_ENOTSUP;
}

int platform_tls_close(int connection)
{
    (void) connection;
    return -TABOS_ENOTSUP;
}

int platform_tls_send(int connection, const void* data, uint32_t size)
{
    (void) connection;
    (void) data;
    (void) size;
    return -TABOS_ENOTSUP;
}

int platform_tls_receive(int connection, void* data, uint32_t capacity)
{
    (void) connection;
    (void) data;
    (void) capacity;
    return -TABOS_ENOTSUP;
}

bool platform_tls_operations_init(void)
{
    return true;
}

void platform_tls_operations_shutdown(void)
{
}

platform_network_operation_result_t platform_network_resolve(const char* hostname, uint32_t family,
                                                             platform_network_address_t* address)
{
    if (hostname == NULL || address == NULL) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    if (strcmp(hostname, "missing.test") == 0) {
        return PLATFORM_NETWORK_OPERATION_NOT_FOUND;
    }
    address->family = family == 6U ? 6U : 4U;
    (void) snprintf(address->text, sizeof(address->text), "%s", address->family == 6U ? "::1" : "127.0.0.1");
    return PLATFORM_NETWORK_OPERATION_OK;
}

platform_network_operation_result_t platform_network_echo(const platform_network_address_t* address, uint16_t sequence,
                                                          uint16_t payload_bytes, uint32_t timeout_ms,
                                                          platform_network_echo_result_t* result)
{
    if (address == NULL || result == NULL || timeout_ms == 0U) {
        return PLATFORM_NETWORK_OPERATION_INVALID;
    }
    if (strcmp(address->text, "198.51.100.1") == 0) {
        return PLATFORM_NETWORK_OPERATION_TIMEOUT;
    }
    result->sequence      = sequence;
    result->bytes         = payload_bytes;
    result->round_trip_ms = 2U;
    return PLATFORM_NETWORK_OPERATION_OK;
}

void test_platform_network_set_state(platform_network_state_t state, const char* failure)
{
    fake_network.state = state;
    (void) snprintf(fake_network.failure, sizeof(fake_network.failure), "%s", failure != NULL ? failure : "");
    if (state == PLATFORM_NETWORK_ONLINE) {
        (void) snprintf(fake_network.ipv4, sizeof(fake_network.ipv4), "192.0.2.10");
        fake_network.signal_dbm = -42;
    }
    if (fake_network_event != NULL) {
        fake_network_event();
    }
}

unsigned int test_platform_network_connect_calls(void)
{
    return network_connect_calls;
}

unsigned int test_platform_network_status_calls(void)
{
    return network_status_calls;
}

const char* test_platform_network_hostname(void)
{
    return network_hostname;
}

void* platform_executable_alloc(size_t size)
{
    return malloc(size);
}

void* platform_executable_prepare(void* memory, size_t size)
{
    return memory != NULL && size > 0U ? memory : NULL;
}

bool platform_executable_finalize(void* memory, size_t size)
{
    return memory != NULL && size > 0U;
}

const void* platform_executable_data_pointer(const void* memory, size_t size)
{
    (void) size;
    return memory;
}

void platform_executable_free(void* memory)
{
    free(memory);
}

bool platform_can_execute_riscv32(void)
{
    return false;
}

platform_riscv32_context_t* platform_riscv32_create(const void* entry, const void* memory, size_t memory_size,
                                                    uint32_t minimum_address, size_t heap_bytes, size_t stack_bytes,
                                                    const tabos_elf_api_t* api, size_t argc, const char* const* argv,
                                                    void* user_data)
{
    (void) entry;
    (void) memory;
    (void) memory_size;
    (void) minimum_address;
    (void) heap_bytes;
    (void) stack_bytes;
    (void) api;
    (void) argc;
    (void) argv;
    (void) user_data;
    return NULL;
}

platform_riscv32_result_t platform_riscv32_step(platform_riscv32_context_t* context, unsigned int instruction_budget,
                                                int* returned_status)
{
    (void) context;
    (void) instruction_budget;
    (void) returned_status;
    return PLATFORM_RISCV32_FAULT;
}

bool platform_riscv32_requires_runtime_slices(void)
{
    return false;
}

uint64_t platform_riscv32_next_deadline(const platform_riscv32_context_t* context)
{
    (void) context;
    return PLATFORM_RUNTIME_DEADLINE_NONE;
}

void platform_riscv32_destroy(platform_riscv32_context_t* context)
{
    (void) context;
}

void* platform_riscv32_current_user_data(void)
{
    return NULL;
}

void test_platform_set_time_ms(uint64_t time_ms)
{
    monotonic_ms = time_ms;
}

void test_platform_advance_time_ms(uint64_t elapsed_ms)
{
    monotonic_ms += elapsed_ms;
}

uint64_t test_platform_time_ms(void)
{
    return monotonic_ms;
}

uint64_t test_platform_runtime_wait_deadline(void)
{
    return fake_runtime_wait_deadline;
}

void platform_input_wait(void)
{
}

bool platform_pointer_init(const char** driver, int* error)
{
    if (driver != NULL) {
        *driver = "fake pointer";
    }
    if (error != NULL) {
        *error = 0;
    }
    return true;
}

void platform_pointer_update(void)
{
    ++pointer_update_calls;
}

unsigned int test_platform_pointer_update_calls(void)
{
    return pointer_update_calls;
}

void platform_pointer_shutdown(void)
{
}

bool platform_pointer_health(int* error)
{
    if (error != NULL) {
        *error = 0;
    }
    return true;
}

static platform_camera_frame_fn fake_camera_frame;
static platform_camera_error_fn fake_camera_error;
static platform_camera_capture_ready_fn fake_camera_ready;

bool platform_camera_init(platform_camera_frame_fn frame, platform_camera_error_fn error,
                          platform_camera_capture_ready_fn capture_ready, platform_camera_info_t* info)
{
    fake_camera_frame = frame;
    fake_camera_error = error;
    fake_camera_ready = capture_ready;
    *info             = (platform_camera_info_t) {.driver     = "fake camera",
                                                  .formats    = TABOS_CAMERA_FORMAT_FLAG_RAW8 | TABOS_CAMERA_FORMAT_FLAG_JPEG,
                                                  .max_width  = 8U,
                                                  .max_height = 8U,
                                                  .max_fps    = 30U,
                                                  .detected   = true,
                                                  .ready      = true};
    return true;
}

bool platform_camera_start(const tabos_camera_config_t* config)
{
    return config != NULL;
}

void platform_camera_stop(void)
{
}

void platform_camera_resume(void)
{
}

void platform_camera_shutdown(void)
{
    fake_camera_frame = NULL;
    fake_camera_error = NULL;
    fake_camera_ready = NULL;
}

void test_platform_camera_frame(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t stride_bytes,
                                uint64_t timestamp_ms)
{
    if (fake_camera_frame != NULL) {
        fake_camera_frame(data, size, width, height, stride_bytes, TABOS_CAMERA_FORMAT_RAW8, timestamp_ms);
    }
}

void test_platform_camera_encoded_frame(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t format,
                                        uint64_t timestamp_ms)
{
    if (fake_camera_frame != NULL) {
        fake_camera_frame(data, size, width, height, 0U, format, timestamp_ms);
    }
}

void test_platform_camera_error(int error)
{
    if (fake_camera_error != NULL) {
        fake_camera_error(error);
    }
}

platform_mutex_t* platform_mutex_create(void)
{
    platform_mutex_t* mutex = calloc(1U, sizeof(*mutex));
    if (mutex != NULL) {
        atomic_flag_clear_explicit(&mutex->locked, memory_order_release);
    }
    return mutex;
}

void platform_mutex_destroy(platform_mutex_t* mutex)
{
    free(mutex);
}

void platform_mutex_lock(platform_mutex_t* mutex)
{
    if (mutex != NULL) {
        while (atomic_flag_test_and_set_explicit(&mutex->locked, memory_order_acquire)) {}
    }
}

void platform_mutex_unlock(platform_mutex_t* mutex)
{
    if (mutex != NULL) {
        atomic_flag_clear_explicit(&mutex->locked, memory_order_release);
    }
}

struct platform_signal {
        bool pending;
};
platform_signal_t* platform_signal_create(void)
{
    return calloc(1U, sizeof(platform_signal_t));
}
void platform_signal_destroy(platform_signal_t* signal)
{
    free(signal);
}
void platform_signal_notify(platform_signal_t* signal)
{
    if (signal != NULL) {
        signal->pending = true;
    }
}
void platform_signal_wait(platform_signal_t* signal, uint32_t timeout_ms)
{
    (void) timeout_ms;
    if (signal != NULL) {
        signal->pending = false;
    }
}

void platform_riscv32_stop(platform_riscv32_context_t* context, void (*cancel)(void*), void* user_data)
{
    (void) context;
    (void) cancel;
    (void) user_data;
}

bool platform_riscv32_current_cancelled(void)
{
    return false;
}

void platform_network_operations_cancel(void)
{
}
void platform_network_socket_operations_cancel(void)
{
}
void platform_tls_operations_cancel(void)
{
}

static unsigned int activity_reports;

void platform_runtime_log_activity(void)
{
    ++activity_reports;
}

unsigned int test_platform_activity_reports(void)
{
    return activity_reports;
}
