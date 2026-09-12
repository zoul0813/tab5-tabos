#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <tabos/application.h>
#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/runtime.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static char storage_root[] = "/tmp/tabos-process-rv32-XXXXXX";
static tabos_app_context_t* parent_context;

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 process test failed: %s\n", message);
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
    *name      = "RV32 process test";
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

int main(int argc, char** argv)
{
    check(argc == 2, "pass SDK-built tester artifact");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char directory[512];
    char executable[512];
    (void) snprintf(directory, sizeof(directory), "%s/bin", storage_root);
    (void) snprintf(executable, sizeof(executable), "%s/bin/tester", storage_root);
    check(mkdir(directory, 0700) == 0, "bin directory");
    copy_file(argv[1], executable);
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0, "headless test backend");
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(application_registry_register(&parent) && tabos_app_launch(parent.name) == TABOS_APP_RESULT_OK,
          "persistent root");
    const char* const arguments[] = {"T:/bin/tester", "--concurrent", NULL};
    check(tabos_app_exec_args(parent_context, arguments[0], 2U, arguments) == TABOS_APP_RESULT_OK, "tester launch");
    const uint64_t deadline = platform_time_ms() + 30000U;
    while (tabos_process_count() > 1U && platform_time_ms() < deadline) {
        kernel_runtime_update(PLATFORM_RUNTIME_EVENT_APPLICATION);
    }
    int status = -1;
    check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "all concurrent children reclaimed");
    check(tabos_app_take_child_status(parent_context, &status) && status == 0, "RV32 concurrency assertions");
    check(tabos_console_write(tabos_app_console(parent_context), "root restored"), "root owns console");
    kernel_runtime_shutdown();
    platform_shutdown();
    check(unlink(executable) == 0 && rmdir(directory) == 0 && rmdir(storage_root) == 0, "fixture cleanup");
    return EXIT_SUCCESS;
}
