#include <tabos/filesystem.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/internal/elf_application.h>
#include <tabos/internal/filesystem.h>
#include <tabos/tty.h>

#include "hello_elf.h"

#include <string.h>

static bool write_file(const char* path, const void* data, size_t size)
{
    const tabos_fd_t file = tabos_fs_open(path, TABOS_O_WRONLY | TABOS_O_CREAT | TABOS_O_TRUNC, 0644U);
    if (file < 0) {
        return false;
    }
    const bool written = tabos_fs_write(file, data, size) == (tabos_ssize_t) size;
    return tabos_fs_close(file) == 0 && written;
}

static bool inherited_path_loads(const char* requested_path, const char* working_directory, const char* expected_path)
{
    const char* const arguments[]         = {requested_path};
    loader_elf_application_t* application = loader_elf_application_create(requested_path, 1U, arguments);
    if (application == NULL || !loader_elf_application_set_working_directory(application, working_directory) ||
        strcmp(loader_elf_application_path(application), expected_path) != 0) {
        loader_elf_application_destroy(application);
        return false;
    }
    loader_elf_image_t image;
    const bool loaded = loader_elf_load_file(loader_elf_application_path(application), &image) == LOADER_ELF_OK;
    if (loaded) {
        loader_elf_unload(&image);
    }
    loader_elf_application_destroy(application);
    return loaded;
}

int main(void)
{
    if (!filesystem_init()) {
        return 1;
    }

    if (tabos_fs_mkdir("A:/bin", 0755U) != 0) {
        filesystem_shutdown();
        return 1;
    }
    const char* const path = "A:/bin/hello.bin";
    if (!write_file(path, loader_hello_elf, loader_hello_elf_size)) {
        filesystem_shutdown();
        return 1;
    }

    loader_elf_info_t inspected;
    const bool query_valid = loader_elf_inspect_file(path, &inspected) == LOADER_ELF_OK &&
                             inspected.launch_flags == 0U && inspected.image_size == 259U &&
                             loader_elf_inspect_file("A:/missing.bin", &inspected) == LOADER_ELF_FILE_OPEN_FAILED;
    loader_elf_image_t image;
    const loader_elf_result_t result = loader_elf_load_file(path, &image);
    const bool valid                 = result == LOADER_ELF_OK && image.memory != NULL && image.entry == image.memory &&
                       image.memory_size == 259U && memcmp(image.memory, loader_hello_elf + 84U, 259U) == 0;
    loader_elf_unload(&image);

    const bool missing_rejected   = loader_elf_load_file("A:/missing.bin", &image) == LOADER_ELF_FILE_OPEN_FAILED;
    const char* const arguments[] = {path};
    loader_elf_application_t* application = loader_elf_application_create(path, 1U, arguments);
    const bool tty_mode_valid             = application != NULL && loader_elf_application_tty_mode(application) == 0U &&
                                loader_elf_application_set_tty_mode(
                                    application, (uint32_t) (TABOS_TTY_MODE_SCROLL_KEYS | TABOS_TTY_MODE_RAW_INPUT)) &&
                                loader_elf_application_tty_mode(application) ==
                                    (uint32_t) (TABOS_TTY_MODE_SCROLL_KEYS | TABOS_TTY_MODE_RAW_INPUT) &&
                                !loader_elf_application_set_tty_mode(application, UINT32_C(0x80000000));
    loader_elf_application_destroy(application);

    static const unsigned char invalid_elf[] = {0U};
    const bool path_directories              = tabos_fs_mkdir("A:/wrong", 0755U) == 0 &&
                                  tabos_fs_mkdir("A:/wrong/bin", 0755U) == 0 && tabos_fs_mkdir("A:/apps", 0755U) == 0 &&
                                  tabos_fs_mkdir("A:/apps/bin", 0755U) == 0;
    const bool path_files = path_directories && write_file("A:/wrong/foo", invalid_elf, sizeof(invalid_elf)) &&
                            write_file("A:/wrong/bin/foo", invalid_elf, sizeof(invalid_elf)) &&
                            write_file("A:/apps/foo", loader_hello_elf, loader_hello_elf_size) &&
                            write_file("A:/apps/bin/foo", loader_hello_elf, loader_hello_elf_size) &&
                            write_file("A:/bin/foo", loader_hello_elf, loader_hello_elf_size) &&
                            tabos_fs_chdir("A:/wrong") == 0;
    const bool inherited_paths = path_files && inherited_path_loads("./foo", "A:/apps", "A:/apps/foo") &&
                                 inherited_path_loads("bin/foo", "A:/apps", "A:/apps/bin/foo") &&
                                 inherited_path_loads("../foo", "A:/apps/bin", "A:/apps/foo") &&
                                 inherited_path_loads("/bin/foo", "A:/apps", "A:/bin/foo");

    (void) tabos_fs_chdir("A:/");
    (void) tabos_fs_unlink("A:/wrong/foo");
    (void) tabos_fs_unlink("A:/wrong/bin/foo");
    (void) tabos_fs_unlink("A:/apps/foo");
    (void) tabos_fs_unlink("A:/apps/bin/foo");
    (void) tabos_fs_unlink("A:/bin/foo");
    (void) tabos_fs_rmdir("A:/wrong/bin");
    (void) tabos_fs_rmdir("A:/wrong");
    (void) tabos_fs_rmdir("A:/apps/bin");
    (void) tabos_fs_rmdir("A:/apps");
    (void) tabos_fs_unlink(path);
    (void) tabos_fs_rmdir("A:/bin");
    filesystem_shutdown();
    return query_valid && valid && missing_rejected && tty_mode_valid && inherited_paths ? 0 : 1;
}
