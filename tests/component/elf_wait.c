#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <tabos/application.h>
#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/runtime.h>
#include <tabos/platform/storage_backend.h>

#include <SDL3/SDL.h>
#include <tabos/device.h>
#include <tabos/internal/device_registry.h>
#include <tabos/internal/network.h>
#include <tabos/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char storage_root[] = "/tmp/tabos-guest-wait-XXXXXX";
static tabos_app_context_t* parent_context;

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "ELF guest wait test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* root, size_t root_size, bool* removable, const char** name)
{
    if (index != 0U || strlen(storage_root) >= root_size) {
        return false;
    }
    strcpy(root, storage_root);
    *letter    = 'T';
    *removable = true;
    *name      = "Guest wait test";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void) letter;
}

bool storage_backend_info(char letter, uint64_t* total_bytes, uint64_t* free_bytes)
{
    (void) letter;
    *total_bytes = 1024U * 1024U;
    *free_bytes  = 512U * 1024U;
    return true;
}

static bool parent_entry(tabos_app_context_t* context)
{
    parent_context = context;
    return tabos_app_console(context) != NULL;
}

static const tabos_app_descriptor_t parent = {
    .abi_version  = TABOS_APPLICATION_ABI_VERSION,
    .name         = "wait-parent",
    .version      = "1",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry        = parent_entry,
};

static void write_u32(uint8_t* bytes, uint32_t value)
{
    for (unsigned int index = 0U; index < 4U; ++index) {
        bytes[index] = (uint8_t) (value >> (8U * index));
    }
}

enum {
    IMAGE_BYTES = 1024,
    ITEMS       = 512,
    ENDPOINT    = 600,
    BUFFER      = 700
};
static uint8_t code[IMAGE_BYTES];
static size_t code_size;
static tabos_device_id_t pointer_id;
static tabos_process_id_t child_id;

typedef enum {
    POINTER_WAIT,
    SOCKET_WAIT,
    MIXED_WAIT,
    SOCKET_RECEIVE,
    NETWORK_RESOLVE,
    COMPUTE_FOREVER
} wait_kind_t;

static void emit(uint32_t instruction)
{
    check(code_size < ITEMS, "fixture code fits");
    write_u32(code + code_size, instruction);
    code_size += 4U;
}

static void immediate(uint32_t reg, int value)
{
    check(value >= -2048 && value <= 2047, "small fixture immediate");
    emit(((uint32_t) value & 0xfffU) << 20U | reg << 7U | 0x13U);
}

static void move(uint32_t to, uint32_t from)
{
    emit(from << 15U | to << 7U | 0x13U);
}

static void call(uint32_t offset)
{
    emit(offset << 20U | 8U << 15U | 2U << 12U | 5U << 7U | 3U);
    emit(0x000280e7U);
}

static void store(uint32_t reg, uint32_t address)
{
    emit((address >> 5U) << 25U | reg << 20U | 2U << 12U | (address & 31U) << 7U | 0x23U);
}

static void write_child(const char* path, wait_kind_t kind, int timeout, uint16_t port)
{
    memset(code, 0, sizeof(code));
    code_size = 0U;
    move(8U, 10U);
    move(9U, 1U);
    const bool pointer = kind == POINTER_WAIT || kind == MIXED_WAIT;
    if (pointer) {
        immediate(10U, (int) pointer_id);
        call(348U); /* pointer_open */
        call(360U); /* pointer_wait_source */
        store(10U, ITEMS);
        write_u32(code + ITEMS + 4U, TABOS_WAIT_READABLE);
    }
    if (kind != POINTER_WAIT && kind != NETWORK_RESOLVE && kind != COMPUTE_FOREVER) {
        immediate(10U, 4);
        immediate(11U, TABOS_SOCKET_UDP);
        call(184U);
        move(18U, 10U); /* s2: socket */
        immediate(11U, ENDPOINT);
        call(192U); /* bind */
        move(10U, 18U);
        call(296U); /* socket_wait_source */
        const uint32_t item = pointer ? ITEMS + 12U : ITEMS;
        store(10U, item);
        write_u32(code + item + 4U, TABOS_WAIT_READABLE);
        write_u32(code + ENDPOINT, 4U);
        strcpy((char*) code + ENDPOINT + 4U, "127.0.0.1");
        write_u32(code + ENDPOINT + 52U, port);
    }
    call(36U); /* yield: test can queue readiness before a zero wait */
    if (kind == COMPUTE_FOREVER) {
        emit(0x0000006fU); /* jal x0, 0: no API gates or cooperative safe points */
    } else if (kind == NETWORK_RESOLVE) {
        strcpy((char*) code + ENDPOINT, "localhost");
        immediate(10U, ENDPOINT);
        immediate(11U, 4);
        immediate(12U, BUFFER);
        call(176U);
    } else if (kind == SOCKET_RECEIVE) {
        move(10U, 18U);
        immediate(11U, BUFFER);
        immediate(12U, 1);
        immediate(13U, 0);
        call(228U); /* blocking recvfrom */
    } else {
        immediate(10U, ITEMS);
        immediate(11U, kind == MIXED_WAIT ? 2 : 1);
        immediate(12U, timeout);
        call(304U);
    }
    emit(0x00048067U); /* return wait result to parent */
    uint8_t elf[84U + IMAGE_BYTES] = {0};
    memcpy(elf, "\177ELF\1\1\1", 7U);
    elf[16] = 2U;
    elf[18] = 243U;
    write_u32(elf + 20U, 1U);
    write_u32(elf + 28U, 52U);
    elf[40] = 52U;
    elf[42] = 32U;
    elf[44] = 1U;
    write_u32(elf + 52U, 1U);
    write_u32(elf + 56U, 84U);
    write_u32(elf + 68U, IMAGE_BYTES);
    write_u32(elf + 72U, IMAGE_BYTES);
    write_u32(elf + 76U, 7U);
    write_u32(elf + 80U, 4U);
    memcpy(elf + 84U, code, sizeof(code));
    FILE* file = fopen(path, "wb");
    check(file != NULL, "open fixture");
    check(fwrite(elf, 1U, sizeof(elf), file) == sizeof(elf), "write fixture");
    check(fclose(file) == 0, "close fixture");
}

static void pump(void)
{
    kernel_runtime_update(platform_runtime_wait_until(kernel_runtime_next_deadline()));
}

static void finish(int expected)
{
    const uint64_t limit = platform_time_ms() + 1000U;
    while (tabos_process_count() > 1U && platform_time_ms() < limit) {
        pump();
    }
    int status = -999;
    check(tabos_process_count() == 1U, "guest completed, parent restored");
    check(tabos_app_take_child_status(parent_context, &status) && status == expected, "guest result");
}

static void launch(const char* path, wait_kind_t kind, int timeout, uint16_t port)
{
    write_child(path, kind, timeout, port);
    check(tabos_app_exec(parent_context, "T:/child") == TABOS_APP_RESULT_OK, "launch child");
    ++child_id;
    kernel_application_system_update(); /* initial yield */
    check(tabos_process_count() == 2U, "child reached yield");
}

static void enter_wait(void)
{
    kernel_application_system_update();
    check(tabos_process_count() == 2U, "waiting child remains alive");
    check(kernel_application_system_next_deadline() > platform_time_ms(), "waiting guest does not busy-spin");
}

static uint16_t reserve_port(void)
{
    const int socket_fd        = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    check(socket_fd >= 0 && bind(socket_fd, (struct sockaddr*) &address, sizeof(address)) == 0,
          "reserve loopback port");
    socklen_t size = sizeof(address);
    check(getsockname(socket_fd, (struct sockaddr*) &address, &size) == 0, "port address");
    close(socket_fd);
    return ntohs(address.sin_port);
}

static void send_byte(uint16_t port)
{
    const int socket_fd              = socket(AF_INET, SOCK_DGRAM, 0);
    const struct sockaddr_in address = {
        .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    check(socket_fd >= 0 && sendto(socket_fd, "x", 1U, 0, (const struct sockaddr*) &address, sizeof(address)) == 1,
          "inject datagram");
    close(socket_fd);
}

int main(void)
{
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0, "headless environment");
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(application_registry_register(&parent) && tabos_app_launch(parent.name) == TABOS_APP_RESULT_OK, "parent");
    tabos_device_info_t device;
    check(device_registry_find(TABOS_DEVICE_NAME_TOUCH, &device), "pointer device");
    pointer_id = device.id;
    check(network_service_connect("test", "", false), "host online");
    network_service_update();
    char path[512];
    (void) snprintf(path, sizeof(path), "%s/child", storage_root);

    launch(path, POINTER_WAIT, 0, 0U);
    finish(0);
    launch(path, POINTER_WAIT, 70, 0U);
    const uint64_t start = platform_time_ms();
    enter_wait();
    finish(0);
    check(platform_time_ms() - start >= 70U, "finite deadline survives repeated runtime turns");

    launch(path, POINTER_WAIT, -1, 0U);
    enter_wait();
    check(kernel_application_power_begin(platform_time_ms()), "freeze blocked pointer wait");
    kernel_application_system_update();
    check(kernel_application_power_status().state == APPLICATION_POWER_PARKED, "wait acknowledges safe parking");
    check(!kernel_application_system_runnable() &&
              kernel_application_system_next_deadline() == PLATFORM_RUNTIME_DEADLINE_NONE,
          "parked wait has no deadline");
    check(tabos_app_exec(parent_context, "T:/child") == TABOS_APP_RESULT_BUSY, "launch admission frozen");
    SDL_Event pointer     = {.type = SDL_EVENT_MOUSE_BUTTON_DOWN};
    pointer.button.button = SDL_BUTTON_LEFT;
    pointer.button.x      = 50.0F;
    pointer.button.y      = 50.0F;
    check(SDL_PushEvent(&pointer), "queue SDL pointer event");
    pump();
    check(tabos_process_count() == 2U && kernel_application_power_status().state == APPLICATION_POWER_PARKED,
          "queued readiness does not execute parked application");
    kernel_application_power_end();
    finish(1);

    launch(path, POINTER_WAIT, 70, 0U);
    enter_wait();
    check(kernel_application_power_begin(platform_time_ms()), "freeze finite wait");
    kernel_application_system_update();
    check(kernel_application_power_status().state == APPLICATION_POWER_PARKED, "finite wait parked");
    SDL_Delay(80U);
    kernel_application_power_end();
    finish(0); /* Original absolute deadline expires; no false cancellation. */

    launch(path, COMPUTE_FOREVER, 0, 0U);
    kernel_application_system_update(); /* Run past initial yield into computation. */
    const uint64_t freeze_start = platform_time_ms();
    check(kernel_application_power_begin(freeze_start), "freeze computing guest");
    kernel_application_system_update();
    check(kernel_application_power_status().state == APPLICATION_POWER_PARKING,
          "instruction slicing does not conceal noncooperation");
    kernel_application_power_update(freeze_start + 1999U);
    check(kernel_application_power_status().state == APPLICATION_POWER_PARKING, "two-second boundary not early");
    kernel_application_power_update(freeze_start + 2000U);
    check(kernel_application_power_status().state == APPLICATION_POWER_TIMEOUT &&
              kernel_application_power_status().blocker == child_id,
          "timeout names stable process blocker");
    check(kernel_application_system_runnable(), "timeout reopens execution");
    check(kernel_process_force_terminate(child_id, 9), "stop computing fixture");
    finish(9);

    launch(path, NETWORK_RESOLVE, 0, 0U);
    enter_wait();
    finish(0);

    for (wait_kind_t kind = SOCKET_WAIT; kind <= SOCKET_RECEIVE; ++kind) {
        const uint16_t port = reserve_port();
        launch(path, kind, -1, port);
        enter_wait();
        send_byte(port);
        finish(1);
        if (kind != SOCKET_RECEIVE) {
            launch(path, kind, 0, port);
            send_byte(port);
            SDL_Delay(20U); /* Allow loopback delivery before the single zero-time poll. */
            finish(1);      /* AUD-002: ready socket-only and mixed zero waits */
        }
    }

    for (unsigned int round = 0U; round < 3U; ++round) {
        launch(path, SOCKET_RECEIVE, -1, reserve_port());
        enter_wait();
        check(kernel_process_force_terminate(child_id, 9), "force blocked socket child");
        finish(9);
        launch(path, NETWORK_RESOLVE, 0, 0U);
        enter_wait();
        check(kernel_process_force_terminate(child_id, 9), "force worker-backed DNS child");
        finish(9);
        launch(path, NETWORK_RESOLVE, 0, 0U);
        enter_wait();
        finish(0); /* A cancelled predecessor cannot deliver into this process. */
    }

    launch(path, POINTER_WAIT, -1, 0U);
    enter_wait();
    SDL_Event quit = {.type = SDL_EVENT_QUIT};
    check(SDL_PushEvent(&quit), "queue SDL shutdown");
    const uint64_t stop_start        = platform_time_ms();
    platform_runtime_events_t events = 0U;
    while ((events & PLATFORM_RUNTIME_EVENT_SHUTDOWN) == 0U && platform_time_ms() - stop_start < 500U) {
        events = platform_runtime_wait_until(kernel_runtime_next_deadline());
        kernel_runtime_update(events);
    }
    check((events & PLATFORM_RUNTIME_EVENT_SHUTDOWN) != 0U, "SDL shutdown reaches blocked guest");
    kernel_runtime_shutdown();
    check(platform_time_ms() - stop_start < 500U, "shutdown cancels infinite wait promptly");
    platform_shutdown();
    check(unlink(path) == 0 && rmdir(storage_root) == 0, "clean storage");
    return EXIT_SUCCESS;
}
