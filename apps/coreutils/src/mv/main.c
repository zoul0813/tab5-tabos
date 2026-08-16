#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: mv source destination\n"); return 1; }
    if (rename(argv[1], argv[2]) != 0) {
        fprintf(stderr, "mv: %s: errno %d\n", argv[1], errno);
        return 1;
    }
    return 0;
}
