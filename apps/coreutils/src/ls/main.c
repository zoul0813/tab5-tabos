#include <dirent.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc > 2) {
        fprintf(stderr, "Usage: ls [path]\n");
        return 2;
    }

    const char* path = argc == 2 ? argv[1] : ".";
    DIR* directory   = opendir(path);
    if (directory == NULL) {
        fprintf(stderr, "ls: cannot open %s (errno %d)\n", path, errno);
        return 1;
    }

    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        printf("%s", entry->d_name);
        if (entry->d_type == DT_DIR) {
            printf("/");
        }
        printf("\n");
    }
    if (closedir(directory) != 0) {
        fprintf(stderr, "ls: close failed (errno %d)\n", errno);
        return 1;
    }
    return 0;
}
