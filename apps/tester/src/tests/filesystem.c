#define _POSIX_C_SOURCE 200809L
#include <tester/test.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char test_directory[] = "T:/tabos-tester";
static const char test_file[]      = "T:/tabos-tester/data.bin";
static const char renamed_file[]   = "T:/tabos-tester/renamed.bin";

void tester_test_filesystem(tester_context_t* context)
{
    static const uint8_t payload[]    = {0x00U, 0x01U, 0x7fU, 0x80U, 0xdbU, 0xffU, '\n'};
    uint8_t received[sizeof(payload)] = {0};
    char original_directory[512];
    char current_directory[512];
    struct stat status;
    int descriptor = -1;

    (void) unlink(test_file);
    (void) unlink(renamed_file);
    (void) rmdir(test_directory);

    tester_expect(context, getcwd(original_directory, sizeof(original_directory)) != NULL,
                  "getcwd reads initial directory");
    tester_expect(context, mkdir(test_directory, 0755) == 0, "mkdir creates directory");
    tester_expect(context, chdir(test_directory) == 0, "chdir selects test directory");
    tester_expect(context,
                  getcwd(current_directory, sizeof(current_directory)) != NULL &&
                      strcmp(current_directory, test_directory) == 0,
                  "getcwd reports process directory");

    descriptor = open("data.bin", O_CREAT | O_RDWR | O_TRUNC, 0644);
    tester_expect(context, descriptor >= 3, "open allocates descriptor 3 or above");
    if (descriptor >= 0) {
        tester_expect(context, write(descriptor, payload, sizeof(payload)) == (ssize_t) sizeof(payload),
                      "write preserves binary and CP437 bytes");
        tester_expect(context, lseek(descriptor, 0, SEEK_SET) == 0, "lseek returns to start");
        tester_expect(context, read(descriptor, received, sizeof(received)) == (ssize_t) sizeof(received),
                      "read returns complete payload");
        tester_expect(context, memcmp(payload, received, sizeof(payload)) == 0, "file bytes round trip unchanged");
        tester_expect(context,
                      fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) &&
                          status.st_size == (off_t) sizeof(payload),
                      "fstat reports regular file size");
        tester_expect(context, close(descriptor) == 0, "close releases descriptor");
        descriptor = -1;
    }

    descriptor = open(test_file, O_RDONLY);
    tester_expect(context, descriptor >= 0, "open for fdopen access validation");
    if (descriptor >= 0) {
        errno = 0;
        FILE* rejected = fdopen(descriptor, "w");
        tester_expect(context, rejected == NULL && errno == EBADF, "fdopen rejects incompatible access");
        if (rejected != NULL) {
            (void) fclose(rejected);
        } else {
            tester_expect(context, fcntl(descriptor, F_GETFL) >= 0, "failed fdopen retains descriptor");
            FILE* stream = fdopen(descriptor, "r");
            tester_expect(context, stream != NULL, "fdopen accepts matching access");
            if (stream != NULL) {
                tester_expect(context, fgetc(stream) == payload[0], "fdopen reads existing data");
                tester_expect(context, fclose(stream) == 0, "fclose owns descriptor");
                errno = 0;
                tester_expect(context, fcntl(descriptor, F_GETFL) == -1 && errno == EBADF,
                              "fclose releases descriptor exactly once");
            } else {
                (void) close(descriptor);
            }
        }
        descriptor = -1;
    }

    tester_expect(context, stat(test_file, &status) == 0 && S_ISREG(status.st_mode), "stat reports file metadata");
    tester_expect(context, rename(test_file, renamed_file) == 0, "rename moves file");
    errno = 0;
    tester_expect(context, stat(test_file, &status) == -1 && errno == ENOENT, "missing file reports ENOENT");
    tester_expect(context, unlink(renamed_file) == 0, "unlink removes file");

    if (descriptor >= 0) {
        (void) close(descriptor);
    }
    tester_expect(context, chdir(original_directory) == 0, "chdir restores initial directory");
    (void) unlink(test_file);
    (void) unlink(renamed_file);
    tester_expect(context, rmdir(test_directory) == 0, "rmdir removes test directory");
}
