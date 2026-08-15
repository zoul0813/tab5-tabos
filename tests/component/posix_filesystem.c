#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <tabos/internal/filesystem.h>

#include <string.h>

int main(void)
{
    if (!tab_fs_init() || mkdir("/portable", 0755U) != 0) return 1;
    const int descriptor = open("/portable/example.txt", O_CREAT | O_RDWR | O_TRUNC, 0644U);
    static const char message[] = "POSIX-compatible TabOS source";
    char buffer[sizeof(message)] = {0};
    if (descriptor < 0 || write(descriptor, message, sizeof(message)) != (ssize_t)sizeof(message) ||
        lseek(descriptor, 0, SEEK_SET) != 0 ||
        read(descriptor, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer) ||
        memcmp(message, buffer, sizeof(message)) != 0) {
        return 1;
    }
    struct stat status;
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_size != sizeof(message) || close(descriptor) != 0) {
        return 1;
    }
    DIR *directory = opendir("/portable");
    struct dirent *entry = directory != NULL ? readdir(directory) : NULL;
    if (entry == NULL || strcmp(entry->d_name, "example.txt") != 0 ||
        closedir(directory) != 0 || unlink("/portable/example.txt") != 0 ||
        rmdir("/portable") != 0) {
        return 1;
    }
    if (open("/missing", O_RDONLY) >= 0 || errno != ENOENT) return 1;
    tab_fs_shutdown();
    if (open("/portable/example.txt", O_RDONLY) >= 0 || errno != ENODEV) return 1;
    return 0;
}
