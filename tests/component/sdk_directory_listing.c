#include <tabos/internal/elf_application.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/elf_api.h>
#include <tabos/posix_compat.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

const tabos_elf_api_t* tabos_runtime_api;
static loader_elf_application_t* application;

static int list_directory(const char* path, char* buffer, uint32_t capacity)
{
    return loader_elf_application_list_directory(application, path, buffer, capacity);
}

int main(void)
{
    assert(filesystem_init());
    const char* const arguments[] = {"A:/probe"};
    application                   = loader_elf_application_create(arguments[0], 1U, arguments);
    assert(application != NULL && tabos_fs_mkdir("A:/listing", 0755U) == 0);
    const char* names[] = {"a\nF:b", "c\rD:d", "colon:%\t", "plain"};
    for (size_t index = 0U; index < 4U; ++index) {
        char path[TABOS_FS_PATH_MAX];
        strcpy(path, "A:/listing/");
        strcat(path, names[index]);
        tabos_fd_t file = tabos_fs_open(path, TABOS_O_CREAT | TABOS_O_WRONLY, 0644U);
        assert(file >= 0 && tabos_fs_close(file) == 0);
    }
    assert(tabos_fs_mkdir("A:/listing/dir\nF:fake", 0755U) == 0);
    const tabos_elf_api_t api    = {.fs_list = list_directory};
    tabos_runtime_api            = &api;
    tabos_posix_dir_t* directory = tabos_posix_opendir("A:/listing");
    assert(directory != NULL);
    unsigned int seen = 0U;
    while (tabos_posix_readdir(directory) != NULL) {
        const char* name = directory->entry.d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        unsigned int index = 0U;
        while (index < 4U && strcmp(name, names[index]) != 0) {
            ++index;
        }
        if (index == 4U) {
            assert(strcmp(name, "dir\nF:fake") == 0 && directory->entry.d_type == 2U);
        } else {
            assert(directory->entry.d_type == 1U);
        }
        assert((seen & (1U << index)) == 0U);
        seen |= 1U << index;
    }
    assert(seen == 31U && tabos_posix_closedir(directory) == 0);
    char record[8];
    assert(tabos_fs_mkdir("A:/single", 0755U) == 0);
    tabos_fd_t file = tabos_fs_open("A:/single/x", TABOS_O_CREAT | TABOS_O_WRONLY, 0644U);
    assert(file >= 0 && tabos_fs_close(file) == 0);
    assert(list_directory("A:/single", record, 3U) == -TABOS_ENOSPC);
    assert(list_directory("A:/single", record, 4U) == 4);
    assert(memcmp(record, "F\1\0x", 4U) == 0);
    assert(tabos_fs_unlink("A:/single/x") == 0 && tabos_fs_rmdir("A:/single") == 0);
    for (size_t index = 0U; index < 4U; ++index) {
        char path[TABOS_FS_PATH_MAX];
        strcpy(path, "A:/listing/");
        strcat(path, names[index]);
        assert(tabos_fs_unlink(path) == 0);
    }
    assert(tabos_fs_rmdir("A:/listing/dir\nF:fake") == 0 && tabos_fs_rmdir("A:/listing") == 0);
    loader_elf_application_destroy(application);
    filesystem_shutdown();
    return 0;
}
