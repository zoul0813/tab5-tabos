#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/application.h>
#include <tabos/internal/input.h>
#include <tabos/internal/runtime.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// This optional executable takes the actual SDK-built shell as argv[1].
// Override only the host drive mapping; runtime, interpreter and SDK stay real.
static char storage_root[] = "/tmp/tabos-shell-rv32-XXXXXX";

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 shell test failed: %s\n", message);
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
    *name      = "Shell test";
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

static void pump(void)
{
    for (unsigned int index = 0U; index < 100U; ++index) {
        // Drain real wake notifications without blocking this bounded test pump.
        const platform_runtime_events_t events = platform_runtime_wait_until(platform_time_ms());
        kernel_runtime_update(events);
    }
}

static void key(tabos_key_t code, uint8_t modifiers)
{
    tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = code, .modifiers = modifiers};
    check(input_submit(&event), "key down");
    event.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&event), "key up");
    if (code == TABOS_KEY_ENTER) {
        const tabos_input_event_t newline = {.type = TABOS_INPUT_TEXT, .text = "\n"};
        check(input_submit(&newline), "normalized enter text");
    }
    pump();
}

static void text(const char* value)
{
    while (*value != '\0') {
        const tabos_input_event_t event = {
            .type = TABOS_INPUT_TEXT, .text = {*value++, '\0'}
        };
        check(input_submit(&event), "text input");
        pump();
    }
}

static void boot(void)
{
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(tabos_app_launch_path("T:/shell") == TABOS_APP_RESULT_OK, "launch real RV32 shell");
    pump();
}

static void stop(void)
{
    kernel_runtime_shutdown();
    platform_shutdown();
}

int main(int argc, char** argv)
{
    check(argc == 2, "pass path to built RV32 shell");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char shell_path[512];
    (void) snprintf(shell_path, sizeof(shell_path), "%s/shell", storage_root);
    FILE* source      = fopen(argv[1], "rb");
    FILE* destination = fopen(shell_path, "wb");
    check(source != NULL && destination != NULL, "open shell artifact");
    char bytes[4096];
    size_t size;
    while ((size = fread(bytes, 1U, sizeof(bytes), source)) > 0U) {
        check(fwrite(bytes, 1U, size, destination) == size, "copy shell");
    }
    check(!ferror(source), "read shell");
    check(fclose(source) == 0 && fclose(destination) == 0, "close shell files");
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0,
          "headless environment");

    boot();
    text("pwd");
    key(TABOS_KEY_ENTER, 0U);
    pump();
    stop();

    char history_path[512];
    (void) snprintf(history_path, sizeof(history_path), "%s/user/history.txt", storage_root);
    FILE* history = fopen(history_path, "rb");
    check(history != NULL && fgets(bytes, sizeof(bytes), history) != NULL && strcmp(bytes, "pwd\n") == 0 &&
              fclose(history) == 0,
          "SDK persisted history");
    boot();
    key(TABOS_KEY_UP, 0U);
    // A trailing space preserves pwd semantics but creates a distinct history
    // entry, making loaded recall observable through the real SDK file path.
    text(" ");
    key(TABOS_KEY_ENTER, 0U);
    pump();
    text("history");
    key(TABOS_KEY_UP, TABOS_MODIFIER_CONTROL);
    key(TABOS_KEY_DOWN, TABOS_MODIFIER_CONTROL);
    key(TABOS_KEY_ENTER, 0U);
    pump();
    stop();
    history = fopen(history_path, "rb");
    check(history != NULL, "restart history");
    size        = fread(bytes, 1U, sizeof(bytes) - 1U, history);
    bytes[size] = '\0';
    check(strcmp(bytes, "pwd\npwd \nhistory\n") == 0 && fclose(history) == 0, "restart recall and Ctrl+Arrow");
    check(unlink(history_path) == 0 && unlink(shell_path) == 0, "clean files");
    char user_path[512];
    (void) snprintf(user_path, sizeof(user_path), "%s/user", storage_root);
    check(rmdir(user_path) == 0 && rmdir(storage_root) == 0, "clean storage");
    puts("RV32 shell history restart passed");
    return EXIT_SUCCESS;
}
