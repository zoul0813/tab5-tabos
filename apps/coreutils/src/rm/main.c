#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: rm file...\n");
        return 1;
    }
    int status = 0;
    for (int index = 1; index < argc; ++index) {
        if (unlink(argv[index]) != 0) {
            fprintf(stderr, "rm: %s: errno %d\n", argv[index], errno);
            status = 1;
        }
    }
    return status;
}
