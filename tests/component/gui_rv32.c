#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <tabos/application.h>
#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/device_registry.h>
#include <tabos/internal/runtime.h>
#include <tabos/platform/storage_backend.h>

#include <tabos/internal/input.h>
#include <tabos/internal/pointer.h>
#include <tabos/internal/surface.h>
#include <tabos/device.h>
#include "hello_elf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static char storage_root[] = "/tmp/tabos-gui-rv32-XXXXXX";
static tabos_app_context_t* parent_context;

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 GUI test failed: %s\n", message);
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
    *name      = "RV32 GUI test";
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
    .name         = "process-parent",
    .version      = "1",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry        = parent_entry,
};

static void copy_file(const char* source, const char* destination)
{
    FILE* input  = fopen(source, "rb");
    FILE* output = fopen(destination, "wb");
    check(input != NULL && output != NULL, "open tester fixture");
    unsigned char bytes[4096];
    size_t count;
    while ((count = fread(bytes, 1U, sizeof(bytes), input)) != 0U) {
        check(fwrite(bytes, 1U, count, output) == count, "copy tester fixture");
    }
    check(!ferror(input), "read tester fixture");
    check(fclose(input) == 0 && fclose(output) == 0, "close tester fixture");
}

static uint16_t pixel_at(unsigned int x, unsigned int y)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    return frame->pixels[(size_t) y * frame->stride_pixels + x];
}

static void pump(void)
{
    kernel_runtime_update(PLATFORM_RUNTIME_EVENT_APPLICATION | PLATFORM_RUNTIME_EVENT_INPUT);
}

static void await_pixel(unsigned int x, unsigned int y, uint16_t color)
{
    const uint64_t deadline = platform_time_ms() + 20000U;
    while (pixel_at(x, y) != color && platform_time_ms() < deadline) {
        pump();
    }
    check(pixel_at(x, y) == color, "expected composed pixel");
}

static void await_count(size_t count)
{
    const uint64_t deadline = platform_time_ms() + 20000U;
    while (tabos_process_count() != count && platform_time_ms() < deadline) {
        pump();
    }
    check(tabos_process_count() == count, "expected process count");
}

static void click(int32_t x, int32_t y)
{
    tabos_device_info_t device;
    check(device_registry_find(TABOS_DEVICE_NAME_TOUCH, &device), "pointer device");
    tabos_pointer_event_t event = {.type       = TABOS_POINTER_DOWN,
                                   .device_id  = device.id,
                                   .contact_id = 0U,
                                   .x          = x,
                                   .y          = y,
                                   .buttons    = TABOS_POINTER_BUTTON_PRIMARY};
    pointer_service_submit(&event);
    event.type    = TABOS_POINTER_UP;
    event.buttons = 0U;
    pointer_service_submit(&event);
}

static uint32_t surface_bytes(void)
{
    surface_transport_packet_t packet = {0};
    check(surface_service_request(1U, SURFACE_TRANSPORT_STATS, &packet, NULL, 0U) == 0, "surface stats");
    return packet.stats.used_bytes;
}

int main(int argc, char** argv)
{
    check(argc == 3, "pass SDK-built desktop and canvas artifacts");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char directory[512], executable[512], canvas[512], hello[512];
    (void) snprintf(directory, sizeof(directory), "%s/bin", storage_root);
    (void) snprintf(executable, sizeof(executable), "%s/bin/desktop", storage_root);
    (void) snprintf(canvas, sizeof(canvas), "%s/bin/canvas", storage_root);
    (void) snprintf(hello, sizeof(hello), "%s/bin/hello", storage_root);
    check(mkdir(directory, 0700) == 0, "bin directory");
    copy_file(argv[1], executable);
    copy_file(argv[2], canvas);
    FILE* legacy = fopen(hello, "wb");
    check(legacy != NULL && fwrite(loader_hello_elf, 1U, loader_hello_elf_size, legacy) == loader_hello_elf_size &&
              fclose(legacy) == 0,
          "legacy fullscreen fixture");
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0, "headless test backend");
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(application_registry_register(&parent) && tabos_app_launch(parent.name) == TABOS_APP_RESULT_OK,
          "persistent root");
    check(tabos_app_exec(parent_context, "T:/bin/desktop") == TABOS_APP_RESULT_OK, "desktop launch");
    await_pixel(1279U, 300U, 0x2b8dU);
    click(1060, 180);
    await_count(3U);
    await_pixel(100U, 200U, 0xffffU);
    click(400, 300);
    await_pixel(400U, 300U, 0x1082U);
    click(1208, 24);
    await_pixel(100U, 200U, 0x2b8dU);
    const uint64_t resized_deadline = platform_time_ms() + 20000U;
    while (surface_bytes() != 960U * 480U * 2U && platform_time_ms() < resized_deadline) {
        pump();
    }
    check(surface_bytes() == 960U * 480U * 2U, "resize adopted and old surface released");
    click(1048, 88);
    await_pixel(100U, 200U, 0xffffU);
    await_pixel(400U, 300U, 0x1082U);
    click(428, 84);
    await_count(4U);
    check(surface_bytes() == 1280U * 592U * 2U, "GUI surface retained during fullscreen execution");
    tabos_process_info_t info;
    check(tabos_process_info(1U, &info) && info.state == TABOS_PROCESS_BLOCKED,
          "desktop retained below fullscreen child");
    await_count(3U);
    await_pixel(400U, 300U, 0x1082U);
    tabos_input_event_t key = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_Q, .modifiers = TABOS_MODIFIER_CONTROL};
    check(input_submit(&key), "close client shortcut");
    key.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&key), "shortcut release");
    await_count(2U);
    check(surface_bytes() == 0U, "client surface cleanup");
    await_pixel(1279U, 300U, 0x2b8dU);
    click(1208, 680);
    await_count(1U);
    int status = -1;
    check(tabos_app_take_child_status(parent_context, &status) && status == 0 && !tabos_process_system_panicked(),
          "desktop exits to persistent root");
    check(tabos_app_console(parent_context) != NULL, "root owns console again");
    kernel_runtime_shutdown();
    platform_shutdown();
    check(unlink(executable) == 0 && unlink(canvas) == 0 && unlink(hello) == 0 && rmdir(directory) == 0 &&
              rmdir(storage_root) == 0,
          "fixture cleanup");
    return 0;
}
