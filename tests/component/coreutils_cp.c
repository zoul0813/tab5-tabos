#define _POSIX_C_SOURCE 200809L

#include <tabos/filesystem.h>
#include <tabos/internal/filesystem.h>

#include "platform_test.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int tabos_cp_main(int argc, char** argv);

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "coreutils cp test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void storage_path(char* destination, size_t capacity, const char* name)
{
    const int length = snprintf(destination, capacity, "%s/data/%s", test_storage_root(), name);
    check(length > 0 && (size_t) length < capacity, "storage path");
}

static void write_file(const char* path, const char* contents)
{
    FILE* file = fopen(path, "wb");
    check(file != NULL, "open fixture for write");
    const size_t length = strlen(contents);
    check(fwrite(contents, 1U, length, file) == length && fclose(file) == 0, "write fixture");
}

static void expect_file(const char* path, const char* expected)
{
    FILE* file = fopen(path, "rb");
    check(file != NULL, "open fixture for read");
    char contents[128];
    const size_t length = fread(contents, 1U, sizeof(contents) - 1U, file);
    contents[length]    = '\0';
    check(!ferror(file) && fclose(file) == 0 && strcmp(contents, expected) == 0, "preserve file contents");
}

int main(void)
{
    check(filesystem_init() && tabos_fs_mkdir("/data", 0755U) == 0 && tabos_fs_chdir("/data") == 0, "filesystem setup");

    char source_path[TABOS_FS_PATH_MAX];
    char alias_path[TABOS_FS_PATH_MAX];
    char destination_path[TABOS_FS_PATH_MAX];
    storage_path(source_path, sizeof(source_path), "source.txt");
    storage_path(alias_path, sizeof(alias_path), "alias.txt");
    storage_path(destination_path, sizeof(destination_path), "destination.txt");
    static const char source_contents[] = "source survives every identity check\n";
    write_file(source_path, source_contents);

    char* direct_arguments[] = {"cp", "A:/data/source.txt", "A:/data/source.txt"};
    check(tabos_cp_main(3, direct_arguments) != 0, "reject identical path");
    expect_file(source_path, source_contents);

    char* normalized_arguments[] = {"cp", "source.txt", "./source.txt"};
    check(tabos_cp_main(3, normalized_arguments) != 0, "reject normalized alias");
    expect_file(source_path, source_contents);

    check(link(source_path, alias_path) == 0, "create host hard link");
    char* hard_link_arguments[] = {"cp", "source.txt", "alias.txt"};
    check(tabos_cp_main(3, hard_link_arguments) != 0, "reject host hard-link alias");
    expect_file(source_path, source_contents);

    write_file(destination_path, "old destination contents that must be truncated\n");
    char* copy_arguments[] = {"cp", "source.txt", "destination.txt"};
    check(tabos_cp_main(3, copy_arguments) == 0, "copy distinct file");
    expect_file(source_path, source_contents);
    expect_file(destination_path, source_contents);

    check(unlink(alias_path) == 0 && unlink(destination_path) == 0 && unlink(source_path) == 0, "remove files");
    char directory_path[TABOS_FS_PATH_MAX];
    const int length = snprintf(directory_path, sizeof(directory_path), "%s/data", test_storage_root());
    check(length > 0 && (size_t) length < sizeof(directory_path) && rmdir(directory_path) == 0, "remove directory");
    filesystem_shutdown();
    return EXIT_SUCCESS;
}
