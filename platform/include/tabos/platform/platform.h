#ifndef TABOS_PLATFORM_PLATFORM_H
#define TABOS_PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/internal/elf_api.h>

enum {
    TABOS_DISPLAY_WIDTH  = 1280,
    TABOS_DISPLAY_HEIGHT = 720,
};

typedef uint16_t platform_pixel_t;
typedef void (*platform_update_fn)(void);
typedef struct platform_riscv32_context platform_riscv32_context_t;
typedef struct platform_mutex platform_mutex_t;

typedef enum {
    PLATFORM_NETWORK_OFFLINE = 0,
    PLATFORM_NETWORK_STARTING,
    PLATFORM_NETWORK_CONNECTING,
    PLATFORM_NETWORK_ONLINE,
    PLATFORM_NETWORK_FAILED,
} platform_network_state_t;

typedef struct {
        platform_network_state_t state;
        char ssid[33];
        char ipv4[16];
        int signal_dbm;
        char failure[64];
} platform_network_status_t;

typedef struct {
        bool available;
        bool charging_enabled;
        bool fast_charging_enabled;
        uint32_t voltage_mv;
        int32_t current_ma;
        int32_t power_mw;
        uint32_t percentage;
        uint32_t charge_state;
} platform_battery_status_t;

typedef enum {
    PLATFORM_NETWORK_OPERATION_OK = 0,
    PLATFORM_NETWORK_OPERATION_INVALID,
    PLATFORM_NETWORK_OPERATION_OFFLINE,
    PLATFORM_NETWORK_OPERATION_NOT_FOUND,
    PLATFORM_NETWORK_OPERATION_TIMEOUT,
    PLATFORM_NETWORK_OPERATION_IO,
    PLATFORM_NETWORK_OPERATION_UNSUPPORTED,
} platform_network_operation_result_t;

typedef struct {
        uint32_t family;
        char text[46];
} platform_network_address_t;

typedef struct {
        uint32_t sequence;
        uint32_t bytes;
        uint32_t round_trip_ms;
} platform_network_echo_result_t;

typedef enum {
    PLATFORM_SYSTEM_ACTION_NONE = 0,
    PLATFORM_SYSTEM_ACTION_REBOOT,
    PLATFORM_SYSTEM_ACTION_POWER_OFF,
} platform_system_action_t;

typedef enum {
    PLATFORM_RISCV32_YIELDED = 0,
    PLATFORM_RISCV32_RETURNED,
    PLATFORM_RISCV32_FAULT,
} platform_riscv32_result_t;

typedef struct {
        platform_pixel_t* pixels;
        size_t width;
        size_t height;
        size_t stride_pixels;
} platform_framebuffer_t;

typedef struct {
        const char* device_name;
        unsigned int cpu_cores;
        unsigned int cpu_frequency_mhz;
        uint64_t memory_total_bytes;
        uint64_t memory_free_bytes;
        bool memory_free_known;
        uint64_t external_memory_total_bytes;
        uint64_t external_memory_free_bytes;
        bool external_memory_present;
        uint64_t flash_capacity_bytes;
        const char* keyboard_name;
        bool keyboard_present;
        const char* rtc_name;
        bool rtc_present;
} platform_diagnostics_t;

bool platform_init(bool headless);
int platform_run(platform_update_fn update);
void platform_shutdown(void);
void platform_stop_run_loop(void);
void platform_perform_system_action(platform_system_action_t action);
const char* platform_name(void);
const char* platform_display_name(void);
bool platform_get_diagnostics(platform_diagnostics_t* diagnostics);
void platform_log(const char* message);
uint64_t platform_time_ms(void);
bool platform_wall_clock_get(int64_t* seconds);
bool platform_wall_clock_set(int64_t seconds);
bool platform_network_init(const char* hostname);
void platform_network_shutdown(void);
bool platform_network_connect(const char* ssid, const char* password);
bool platform_network_disconnect(void);
bool platform_network_status(platform_network_status_t* status);
bool platform_battery_status(platform_battery_status_t* status);
bool platform_battery_set_charging(bool enabled);
bool platform_battery_set_fast_charging(bool enabled);
bool platform_network_operations_init(void);
void platform_network_operations_shutdown(void);
bool platform_network_socket_operations_init(void);
void platform_network_socket_operations_shutdown(void);
void platform_network_socket_interrupt(int socket);
bool platform_network_socket_operations_suspend(void);
void platform_network_socket_operations_resume(void);
void platform_network_socket_dispose(int socket);
platform_network_operation_result_t platform_network_resolve(const char* hostname, uint32_t family,
                                                             platform_network_address_t* address);
platform_network_operation_result_t platform_network_echo(const platform_network_address_t* address, uint16_t sequence,
                                                          uint16_t payload_bytes, uint32_t timeout_ms,
                                                          platform_network_echo_result_t* result);
int platform_network_socket_open(uint32_t family, uint32_t type);
int platform_network_socket_close(int socket);
int platform_network_socket_bind(int socket, const platform_network_address_t* address, uint16_t port);
int platform_network_socket_get_local_endpoint(int socket, platform_network_address_t* address, uint16_t* port);
int platform_network_socket_listen(int socket, uint16_t backlog);
int platform_network_socket_accept(int socket, platform_network_address_t* address, uint16_t* port);
int platform_network_socket_connect(int socket, const platform_network_address_t* address, uint16_t port);
int platform_network_socket_set_nonblocking(int socket, bool enabled);
int platform_network_socket_shutdown(int socket, uint32_t direction);
int platform_network_socket_send(int socket, const void* data, uint32_t size);
int platform_network_socket_receive(int socket, void* data, uint32_t capacity);
int platform_network_socket_send_to(int socket, const void* data, uint32_t size,
                                    const platform_network_address_t* address, uint16_t port);
int platform_network_socket_receive_from(int socket, void* data, uint32_t capacity, platform_network_address_t* address,
                                         uint16_t* port);
uint32_t platform_graphics_capabilities(void);
bool platform_graphics_begin(void);
void platform_graphics_end(void);
bool platform_graphics_present(platform_framebuffer_t* framebuffer);
bool platform_graphics_fill(platform_framebuffer_t* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height,
                            platform_pixel_t color);
bool platform_graphics_blit(platform_framebuffer_t* framebuffer, const tabos_graphics_blit_options_t* options);
bool platform_raster_fill_span(platform_pixel_t* destination, size_t count, platform_pixel_t color);
bool platform_raster_copy_span(platform_pixel_t* destination, const platform_pixel_t* source, size_t count);
void platform_raster_diagnostics(void);
void* platform_executable_alloc(size_t size);
void* platform_executable_prepare(void* memory, size_t size);
bool platform_executable_finalize(void* memory, size_t size);
const void* platform_executable_data_pointer(const void* memory, size_t size);
void platform_executable_free(void* memory);
bool platform_can_execute_riscv32(void);
platform_riscv32_context_t* platform_riscv32_create(const void* entry, const void* memory, size_t memory_size,
                                                    uint32_t minimum_address, const tabos_elf_api_t* api, size_t argc,
                                                    const char* const* argv, void* user_data);
platform_riscv32_result_t platform_riscv32_step(platform_riscv32_context_t* context, unsigned int instruction_budget,
                                                int* returned_status);
void platform_riscv32_destroy(platform_riscv32_context_t* context);
void* platform_riscv32_current_user_data(void);
void platform_input_wait(void);
platform_mutex_t* platform_mutex_create(void);
void platform_mutex_destroy(platform_mutex_t* mutex);
void platform_mutex_lock(platform_mutex_t* mutex);
void platform_mutex_unlock(platform_mutex_t* mutex);

bool platform_display_init(platform_framebuffer_t* framebuffer);
bool platform_display_present(const platform_framebuffer_t* framebuffer);
void platform_display_shutdown(void);

#endif
