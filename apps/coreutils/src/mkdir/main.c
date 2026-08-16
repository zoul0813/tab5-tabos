#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: mkdir <path>\n");
        return 2;
    }
    if (mkdir(argv[1], 0755) != 0) {
        fprintf(stderr, "mkdir: cannot create %s (errno %d)\n", argv[1], errno);
        return 1;
    }
    return 0;
}
