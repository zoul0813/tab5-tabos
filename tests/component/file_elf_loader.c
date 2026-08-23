#include <tabos/filesystem.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/internal/elf_application.h>
#include <tabos/internal/filesystem.h>
#include <tabos/tty.h>

#include "hello_elf.h"

#include <string.h>

int main(void)
{
    if (!filesystem_init()) return 1;

    if (tabos_fs_mkdir("A:/bin", 0755U) != 0) {
        filesystem_shutdown();
        return 1;
    }
    const char *const path = "A:/bin/hello.bin";
    const tabos_fd_t file = tabos_fs_open(
        path, TABOS_O_WRONLY | TABOS_O_CREAT | TABOS_O_TRUNC, 0644U);
    if (file < 0 ||
        tabos_fs_write(file, loader_hello_elf, loader_hello_elf_size) !=
            (tabos_ssize_t)loader_hello_elf_size ||
        tabos_fs_close(file) != 0) {
        filesystem_shutdown();
        return 1;
    }

    loader_elf_image_t image;
    const loader_elf_result_t result = loader_elf_load_file(path, &image);
    const bool valid = result == LOADER_ELF_OK && image.memory != NULL &&
        image.entry == image.memory && image.memory_size == 259U &&
        memcmp(image.memory, loader_hello_elf + 84U, 259U) == 0;
    loader_elf_unload(&image);

    const bool missing_rejected =
        loader_elf_load_file("A:/missing.bin", &image) == LOADER_ELF_FILE_OPEN_FAILED;
    const char *const arguments[] = {path};
    loader_elf_application_t *application =
        loader_elf_application_create(path, 1U, arguments);
    const bool tty_mode_valid = application != NULL &&
        loader_elf_application_tty_mode(application) == 0U &&
        loader_elf_application_set_tty_mode(
            application, (uint32_t)TABOS_TTY_MODE_SCROLL_KEYS) &&
        loader_elf_application_tty_mode(application) ==
            (uint32_t)TABOS_TTY_MODE_SCROLL_KEYS &&
        !loader_elf_application_set_tty_mode(application, UINT32_C(0x80000000));
    loader_elf_application_destroy(application);
    (void)tabos_fs_unlink(path);
    (void)tabos_fs_rmdir("A:/bin");
    filesystem_shutdown();
    return valid && missing_rejected && tty_mode_valid ? 0 : 1;
}
