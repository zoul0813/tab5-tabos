#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/application.h>
#include <tabos/internal/input.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/application.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// This optional executable takes the actual SDK-built shell as argv[1].
// Override only the host drive mapping; runtime, interpreter and SDK stay real.
static char storage_root[] = "/tmp/tabos-kilo-rv32-XXXXXX";

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 Kilo test failed: %s\n", message);
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
        const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        check(input_submit(&down), "text physical key");
        const tabos_input_event_t event = {
            .type = TABOS_INPUT_TEXT, .text = {*value++, '\0'}
        };
        check(input_submit(&event), "text input");
        const tabos_input_event_t up = {.type = TABOS_INPUT_KEY_UP, .key = TABOS_KEY_A};
        check(input_submit(&up), "text key release");
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


static void copy(const char* src, const char* dst)
{
    FILE* source      = fopen(src, "rb");
    FILE* destination = fopen(dst, "wb");
    check(source != NULL && destination != NULL, "open artifact");
    char bytes[4096];
    size_t size;
    while ((size = fread(bytes, 1U, sizeof(bytes), source)) > 0U) {
        check(fwrite(bytes, 1U, size, destination) == size, "copy artifact");
    }
    check(!ferror(source) && fclose(source) == 0 && fclose(destination) == 0, "close artifact");
}
static void child(void)
{
    for (size_t i = 0U; i < 100U && (tabos_process_count() != 2U || kernel_application_system_runnable()); ++i) {
        pump();
    }
    check(tabos_process_count() == 2U, "Kilo is shell child");
    check(!kernel_application_system_runnable(), "idle RV32 keyboard wait suspends guest");
    check(kernel_runtime_next_deadline() > platform_time_ms(), "idle wait does not spin runtime");
}
static void parent(void)
{
    for (size_t i = 0U; i < 100U && tabos_process_count() != 1U; ++i) {
        pump();
    }
    int status = -1;
    check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "parent shell resumed");
    check(tabos_app_last_exit_status(&status) && status == 0, "Kilo exits zero");
}
int main(int argc, char** argv)
{
    check(argc == 3 || argc == 4, "pass SDK shell, Kilo, and optional tester artifacts");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char shell_path[512], kilo_path[512], file_path[512], history_path[512], user_path[512];
    (void) snprintf(shell_path, sizeof(shell_path), "%s/shell", storage_root);
    (void) snprintf(kilo_path, sizeof(kilo_path), "%s/kilo", storage_root);
    (void) snprintf(file_path, sizeof(file_path), "%s/a space.c", storage_root);
    (void) snprintf(history_path, sizeof(history_path), "%s/user/history.txt", storage_root);
    (void) snprintf(user_path, sizeof(user_path), "%s/user", storage_root);
    copy(argv[1], shell_path);
    copy(argv[2], kilo_path);
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0,
          "headless environment");
    char tester_path[512], bin_path[512];
    (void) snprintf(bin_path, sizeof(bin_path), "%s/bin", storage_root);
    (void) snprintf(tester_path, sizeof(tester_path), "%s/bin/tester", storage_root);
    if (argc == 4) {
        check(mkdir(bin_path, 0700) == 0, "create application PATH directory");
        copy(argv[3], tester_path);
    }
    boot();
    text("./kilo 'a space.c'");
    key(TABOS_KEY_ENTER, 0U);
    child();
    text("int x;");
    key(TABOS_KEY_ENTER, 0U);
    text("// note");
    key(TABOS_KEY_F, TABOS_MODIFIER_CONTROL);
    text("int");
    key(TABOS_KEY_ESCAPE, 0U);
    char collision[512];
    for (unsigned i = 0U; i < 64U; ++i) {
        (void) snprintf(collision, sizeof(collision), "%s/.kilo-%u.tmp", storage_root, i);
        check(mkdir(collision, 0700) == 0, "force staging collision");
    }
    key(TABOS_KEY_S, TABOS_MODIFIER_CONTROL);
    child();
    check(access(file_path, F_OK) != 0, "failed save leaves missing file absent");
    for (unsigned i = 0U; i < 64U; ++i) {
        (void) snprintf(collision, sizeof(collision), "%s/.kilo-%u.tmp", storage_root, i);
        check(rmdir(collision) == 0, "clear staging collision");
    }
    key(TABOS_KEY_S, TABOS_MODIFIER_CONTROL);
    child();
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    FILE* file      = fopen(file_path, "rb");
    char bytes[128] = {0};
    check(file != NULL, "saved file exists");
    size_t size = fread(bytes, 1U, sizeof(bytes), file);
    check(fclose(file) == 0 && size == 14U && memcmp(bytes, "int x;\n// note", size) == 0, "actual RV32 saved bytes");
    text("./kilo 'a space.c'");
    key(TABOS_KEY_ENTER, 0U);
    child();
    text("discard");
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    child();
    key(TABOS_KEY_N, 0U);
    child();
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    key(TABOS_KEY_Y, 0U);
    parent();
    file = fopen(file_path, "rb");
    check(file != NULL, "reopen after discard");
    size = fread(bytes, 1U, sizeof(bytes), file);
    check(fclose(file) == 0 && size == 14U && memcmp(bytes, "int x;\n// note", size) == 0,
          "discard leaves saved bytes");
    text("pwd");
    key(TABOS_KEY_ENTER, 0U);
    pump();
    check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "working parent command loop");
    char large_path[512];
    (void) snprintf(large_path, sizeof(large_path), "%s/large.txt", storage_root);
    file = fopen(large_path, "wb");
    check(file != NULL, "large fixture");
    for (size_t i = 0U; i < 8000U; ++i) {
        check(fputs("\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n", file) >= 0, "large tab row");
    }
    check(fclose(file) == 0, "close large fixture");
    text("./kilo large.txt");
    key(TABOS_KEY_ENTER, 0U);
    child();
    text("X");
    key(TABOS_KEY_S, TABOS_MODIFIER_CONTROL);
    child();
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    struct stat large_info;
    check(stat(large_path, &large_info) == 0 && large_info.st_size == 256001,
          "near-limit RV32 heap and streaming save");
    check(unlink(large_path) == 0, "remove large fixture");
    if (argc == 4) {
        text("tester --input");
        key(TABOS_KEY_ENTER, 0U);
        for (size_t i = 0U; i < 500U && tabos_process_count() != 1U; ++i) {
            pump();
        }
        parent();
        text("./bin/tester --input");
        key(TABOS_KEY_ENTER, 0U);
        for (size_t i = 0U; i < 500U && tabos_process_count() != 1U; ++i) {
            pump();
        }
        parent();
        check(unlink(tester_path) == 0, "remove tester artifact");
        check(rmdir(bin_path) == 0, "remove application PATH directory");
    }
    text("./kilo 'a space.c'");
    key(TABOS_KEY_ENTER, 0U);
    child();
    // Forced runtime teardown must cancel and release a suspended input wait.
    stop();
    boot();
    text("./kilo 'a space.c'");
    key(TABOS_KEY_ENTER, 0U);
    child();
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    stop();
    check(unlink(file_path) == 0 && unlink(kilo_path) == 0 && unlink(shell_path) == 0 && unlink(history_path) == 0,
          "clean files");
    check(rmdir(user_path) == 0 && rmdir(storage_root) == 0, "clean root");
    puts("RV32 Kilo edit/search/save/reopen/discard and idle suspension passed");
    return 0;
}
