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
#include <dirent.h>

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
    if (pixel_at(x, y) != color) {
        fprintf(stderr, "pixel (%u,%u): got %04x expected %04x\n", x, y, pixel_at(x, y), color);
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

static void settle(void)
{
    const uint64_t deadline = platform_time_ms() + 500U;
    while (platform_time_ms() < deadline) {
        pump();
    }
}

static void capture_frame(void)
{
    const char* path = getenv("TABOS_GUI_TEST_CAPTURE");
    if (path == NULL) {
        return;
    }
    FILE* file = fopen(path, "wb");
    check(file != NULL, "open optional framebuffer capture");
    (void) fprintf(file, "P6\n1280 720\n255\n");
    for (unsigned int y = 0U; y < 720U; ++y) {
        for (unsigned int x = 0U; x < 1280U; ++x) {
            const uint16_t color       = pixel_at(x, y);
            const unsigned char rgb[3] = {(unsigned char) (((color >> 11U) & 31U) * 255U / 31U),
                                          (unsigned char) (((color >> 5U) & 63U) * 255U / 63U),
                                          (unsigned char) ((color & 31U) * 255U / 31U)};
            check(fwrite(rgb, 1U, 3U, file) == 3U, "write framebuffer capture");
        }
    }
    check(fclose(file) == 0, "close framebuffer capture");
}

static void close_shortcut(void)
{
    tabos_input_event_t key = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_Q, .modifiers = TABOS_MODIFIER_CONTROL};
    check(input_submit(&key), "close shortcut");
    key.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&key), "close release");
}

static void finish_optional_game(void)
{
    if (getenv("TABOS_GUI_TEST_FULLSCREEN") == NULL) {
        return;
    }
    const bool doom         = getenv("TABOS_GUI_TEST_IWAD") != NULL;
    const uint64_t deadline = platform_time_ms() + (doom ? 5000U : 500U);
    while (platform_time_ms() < deadline) {
        pump();
    }
    tabos_input_event_t key = {.type = TABOS_INPUT_KEY_DOWN, .key = doom ? TABOS_KEY_F10 : TABOS_KEY_Q};
    check(input_submit(&key), "game quit");
    key.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&key), "game quit release");
    if (doom) {
        settle();
        key.type = TABOS_INPUT_KEY_DOWN;
        key.key  = TABOS_KEY_Y;
        check(input_submit(&key), "Doom quit confirmation");
        key.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&key), "Doom quit confirmation release");
    }
}

static void remove_fixture_tree(const char* path)
{
    DIR* directory = opendir(path);
    check(directory != NULL, "open fixture directory for cleanup");
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[1024];
        const int length = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        check(length > 0 && (size_t) length < sizeof(child), "fixture child path");
        struct stat info;
        check(lstat(child, &info) == 0, "fixture child metadata");
        if (S_ISDIR(info.st_mode)) {
            remove_fixture_tree(child);
        } else {
            check(unlink(child) == 0, "fixture file cleanup");
        }
    }
    check(closedir(directory) == 0 && rmdir(path) == 0, "fixture directory cleanup");
}

int main(int argc, char** argv)
{
    check(argc == 6, "pass SDK-built desktop, canvas, files, calculator and editor artifacts");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char directory[512], executable[512], canvas[512], hello[512];
    (void) snprintf(directory, sizeof(directory), "%s/bin", storage_root);
    (void) snprintf(executable, sizeof(executable), "%s/bin/desktop", storage_root);
    (void) snprintf(canvas, sizeof(canvas), "%s/bin/canvas", storage_root);
    (void) snprintf(hello, sizeof(hello), "%s/bin/hello", storage_root);
    check(mkdir(directory, 0700) == 0, "bin directory");
    copy_file(argv[1], executable);
    copy_file(argv[2], canvas);
    const char* extra_names[] = {"files", "calculator", "editor"};
    char extras[3][512], note[512];
    for (size_t index = 0U; index < 3U; ++index) {
        (void) snprintf(extras[index], sizeof(extras[index]), "%s/bin/%s", storage_root, extra_names[index]);
        copy_file(argv[index + 3U], extras[index]);
    }
    (void) snprintf(note, sizeof(note), "%s/notes.txt", storage_root);
    FILE* legacy = fopen(hello, "wb");
    check(legacy != NULL && fwrite(loader_hello_elf, 1U, loader_hello_elf_size, legacy) == loader_hello_elf_size &&
              fclose(legacy) == 0,
          "legacy fullscreen fixture");
    const char* game = getenv("TABOS_GUI_TEST_FULLSCREEN");
    if (game != NULL) {
        copy_file(game, hello);
    }
    const char* iwad = getenv("TABOS_GUI_TEST_IWAD");
    char data[512], doom_data[512], wad[512];
    (void) snprintf(data, sizeof(data), "%s/data", storage_root);
    if (iwad != NULL) {
        (void) snprintf(doom_data, sizeof(doom_data), "%s/data/doom", storage_root);
        (void) snprintf(wad, sizeof(wad), "%s/data/doom/freedoom2.wad", storage_root);
        check(mkdir(data, 0700) == 0 && mkdir(doom_data, 0700) == 0, "game fixture directory");
        copy_file(iwad, wad);
    }
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
    finish_optional_game();
    await_count(3U);
    await_pixel(400U, 300U, 0x1082U);
    if (game != NULL) {
        int game_status = -1;
        check(tabos_app_last_exit_status(&game_status) && game_status == 0, "fullscreen game exited successfully");
    }
    tabos_input_event_t key = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_Q, .modifiers = TABOS_MODIFIER_CONTROL};
    check(input_submit(&key), "close client shortcut");
    key.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&key), "shortcut release");
    await_count(2U);
    check(surface_bytes() == 0U, "client surface cleanup");
    await_pixel(1279U, 300U, 0x2b8dU);
    click(770, 180);
    await_count(3U);
    await_pixel(8U, 112U, 0x632cU);
    click(40, 150);
    tabos_input_event_t text = {.type = TABOS_INPUT_TEXT, .text = "retained"};
    check(input_submit(&text), "editor text");
    settle();
    click(220, 80);
    settle();
    click(390, 352);
    const uint64_t save_deadline = platform_time_ms() + 20000U;
    struct stat saved;
    while (stat(note, &saved) != 0 && platform_time_ms() < save_deadline) {
        pump();
    }
    check(stat(note, &saved) == 0 && saved.st_size == 8, "editor save through real public filesystem");
    settle();
    click(160, 150);
    text.text[0] = '!';
    text.text[1] = '\0';
    check(input_submit(&text), "dirty editor text");
    settle();
    click(1160, 24);
    await_pixel(1279U, 300U, 0x2b8dU);
    click(1060, 180);
    await_count(4U);
    await_pixel(100U, 200U, 0xffffU);
    click(428, 84);
    await_count(5U);
    check(surface_bytes() == 2U * 1280U * 592U * 2U, "dirty editor and canvas remain resident during handoff");
    finish_optional_game();
    await_count(4U);
    if (game != NULL) {
        int game_status = -1;
        check(tabos_app_last_exit_status(&game_status) && game_status == 0,
              "second fullscreen game exited successfully");
    }
    settle();
    close_shortcut();
    await_count(3U);
    click(196, 680);
    settle();
    click(1208, 680);
    settle();
    capture_frame();
    click(640, 352);
    settle();
    check(tabos_process_count() == 3U, "dirty close cancellation retains editor");
    close_shortcut();
    settle();
    click(900, 352);
    await_count(2U);
    FILE* saved_file    = fopen(note, "rb");
    char saved_text[16] = {0};
    check(saved_file != NULL && fread(saved_text, 1U, sizeof(saved_text), saved_file) == 8U &&
              fclose(saved_file) == 0 && strcmp(saved_text, "retained") == 0,
          "discard leaves saved file unchanged");
    await_pixel(1279U, 300U, 0x2b8dU);
    click(480, 180);
    await_count(3U);
    settle();
    close_shortcut();
    await_count(2U);
    await_pixel(1279U, 300U, 0x2b8dU);
    click(180, 180);
    await_count(3U);
    settle();
    close_shortcut();
    await_count(2U);
    await_pixel(1279U, 300U, 0x2b8dU);
    click(770, 180);
    await_count(3U);
    await_pixel(8U, 112U, 0x632cU);
    click(40, 150);
    text = (tabos_input_event_t) {.type = TABOS_INPUT_TEXT, .text = "unsaved"};
    check(input_submit(&text), "force-close dirty text");
    settle();
    close_shortcut();
    settle();
    click(1250, 24);
    settle();
    click(750, 448);
    await_count(2U);
    check(surface_bytes() == 0U, "explicit force close reclaims client surface");
    await_pixel(1279U, 300U, 0x2b8dU);
    click(1208, 680);
    await_count(1U);
    int status = -1;
    check(tabos_app_take_child_status(parent_context, &status) && status == 0 && !tabos_process_system_panicked(),
          "desktop exits to persistent root");
    check(tabos_app_console(parent_context) != NULL, "root owns console again");
    for (unsigned int failure = 0U; failure < 2U; ++failure) {
        check(tabos_app_exec(parent_context, "T:/bin/desktop") == TABOS_APP_RESULT_OK, "relaunch desktop for recovery");
        await_pixel(1279U, 300U, 0x2b8dU);
        tabos_process_id_t desktop_pid = 0U;
        for (tabos_process_id_t id = 1U; id < 64U; ++id) {
            if (tabos_process_info(id, &info) && info.parent_id == 0U) {
                desktop_pid = id;
                break;
            }
        }
        check(desktop_pid != 0U, "desktop recovery identity");
        click(1060, 180);
        await_count(3U);
        await_pixel(100U, 200U, 0xffffU);
        if (failure != 0U) {
            click(428, 84);
            await_count(4U);
        }
        check(kernel_process_force_terminate(desktop_pid, 77), "recoverable desktop termination");
        await_count(1U);
        check(tabos_app_take_child_status(parent_context, &status) && status == 77 && surface_bytes() == 0U &&
                  tabos_app_console(parent_context) != NULL,
              "desktop recovery tears down GUI and fullscreen descendants");
    }
    kernel_runtime_shutdown();
    platform_shutdown();
    for (size_t index = 0U; index < 3U; ++index) {
        check(unlink(extras[index]) == 0, "extra client cleanup");
    }
    check(unlink(note) == 0, "document fixture cleanup");
    struct stat data_info;
    if (stat(data, &data_info) == 0) {
        remove_fixture_tree(data);
    }
    check(unlink(executable) == 0 && unlink(canvas) == 0 && unlink(hello) == 0 && rmdir(directory) == 0 &&
              rmdir(storage_root) == 0,
          "fixture cleanup");
    return 0;
}
