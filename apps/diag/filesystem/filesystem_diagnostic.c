#include <tabos/internal/filesystem_diagnostic.h>
#include <tabos/internal/application.h>

#include <tabos/tabos.h>

#include <string.h>

static const char test_directory[] = "/tabos-fs-test";
static const char test_file[] = "/tabos-fs-test/original.txt";
static const char renamed_file[] = "/tabos-fs-test/renamed.txt";
static const char payload[] = "TabOS filesystem diagnostic payload\n";

static const tabos_console_session_t *console;

static char *append_text(char *output, const char *end, const char *text)
{
    while (*text != '\0' && output < end) {
        *output++ = *text++;
    }
    return output;
}

static char *append_error_number(char *output, const char *end, int error)
{
    char digits[11];
    size_t count = 0U;
    unsigned int value = error < 0 ? (unsigned int)(-(error + 1)) + 1U : (unsigned int)error;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));

    if (error < 0 && output < end) {
        *output++ = '-';
    }
    while (count > 0U && output < end) {
        *output++ = digits[--count];
    }
    return output;
}

static void report(const char *operation, bool passed)
{
    char line[96] = {0};
    char *output = line;
    const char *const end = line + sizeof(line) - 1U;

    output = append_text(output, end, passed ? "[OK] " : "[FAIL] ");
    output = append_text(output, end, operation);
    if (!passed) {
        const int error = *tabos_errno_location();
        output = append_text(output, end, " (errno ");
        output = append_error_number(output, end, error);
        output = append_text(output, end, ")");
    }
    output = append_text(output, end, "\n");
    *output = '\0';
    (void)tabos_console_write(console, line);
}

static void remove_previous_test(void)
{
    (void)tabos_fs_unlink(test_file);
    (void)tabos_fs_unlink(renamed_file);
    (void)tabos_fs_rmdir(test_directory);
}

static bool run_diagnostic(void)
{
    bool passed = true;
    tabos_fd_t descriptor = -1;
    tabos_dir_t directory = -1;

#define CHECK(operation, expression) do { \
        const bool result = (expression); \
        report((operation), result); \
        if (!result) { passed = false; goto cleanup; } \
    } while (0)

    remove_previous_test();
    CHECK("Create directory", tabos_fs_mkdir(test_directory, 0755U) == 0);

    descriptor = tabos_fs_open(test_file,
        TABOS_O_CREAT | TABOS_O_RDWR | TABOS_O_TRUNC, 0644U);
    CHECK("Create file", descriptor >= 0);
    CHECK("Write payload",
        tabos_fs_write(descriptor, payload, sizeof(payload)) == (tabos_ssize_t)sizeof(payload));
    CHECK("Seek to start", tabos_fs_seek(descriptor, 0, TABOS_SEEK_SET) == 0);

    char buffer[sizeof(payload)] = {0};
    CHECK("Read payload",
        tabos_fs_read(descriptor, buffer, sizeof(buffer)) == (tabos_ssize_t)sizeof(buffer));
    CHECK("Verify payload", memcmp(buffer, payload, sizeof(payload)) == 0);

    tabos_stat_t status;
    CHECK("Inspect open file",
        tabos_fs_fstat(descriptor, &status) == 0 &&
        (status.mode & TABOS_S_IFREG) != 0U && status.size == sizeof(payload));
    CHECK("Close file", tabos_fs_close(descriptor) == 0);
    descriptor = -1;

    descriptor = tabos_fs_open(test_file, TABOS_O_RDONLY, 0U);
    CHECK("Reopen file", descriptor >= 0);
    memset(buffer, 0, sizeof(buffer));
    CHECK("Read reopened file",
        tabos_fs_read(descriptor, buffer, sizeof(buffer)) == (tabos_ssize_t)sizeof(buffer) &&
        memcmp(buffer, payload, sizeof(payload)) == 0);
    CHECK("Close reopened file", tabos_fs_close(descriptor) == 0);
    descriptor = -1;

    CHECK("Inspect file path",
        tabos_fs_stat(test_file, &status) == 0 && status.size == sizeof(payload));
    CHECK("Rename file", tabos_fs_rename(test_file, renamed_file) == 0);

    directory = tabos_fs_opendir(test_directory);
    CHECK("Open directory", directory >= 0);
    bool found = false;
    tabos_dirent_t entry;
    int read_result = 0;
    while ((read_result = tabos_fs_readdir(directory, &entry)) > 0) {
        if (strcmp(entry.name, "renamed.txt") == 0) found = true;
    }
    CHECK("Enumerate directory", read_result == 0 && found);
    CHECK("Close directory", tabos_fs_closedir(directory) == 0);
    directory = -1;

    CHECK("Remove file", tabos_fs_unlink(renamed_file) == 0);
    CHECK("Remove directory", tabos_fs_rmdir(test_directory) == 0);

cleanup:
    if (descriptor >= 0) (void)tabos_fs_close(descriptor);
    if (directory >= 0) (void)tabos_fs_closedir(directory);
    if (passed) remove_previous_test();
#undef CHECK
    return passed;
}

static bool diagnostic_entry(tabos_app_context_t *context)
{
    console = tabos_app_console(context);
    if (console == NULL) return false;
    (void)tabos_console_write(console, "\nFilesystem diagnostic\n");
    const bool passed = run_diagnostic();
    (void)tabos_console_write(console,
        passed ? "Filesystem diagnostic passed\n" : "Filesystem diagnostic failed\n");
    tab_app_report_diagnostic_result(context, passed ? 0 : 1);
    return true;
}

static void diagnostic_cleanup(tabos_app_context_t *context, int exit_status)
{
    (void)context;
    (void)exit_status;
    console = NULL;
}

const tabos_app_descriptor_t tab_filesystem_diagnostic_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "filesystem-test",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = diagnostic_entry,
    .update = NULL,
    .cleanup = diagnostic_cleanup,
};
