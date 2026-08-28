#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: reboot\n");
        return 0;
    }
    if (argc != 1) {
        fprintf(stderr, "Usage: reboot\n");
        return 1;
    }
    printf("Rebooting...\n");
    if (reboot(RB_AUTOBOOT) != 0) {
        fprintf(stderr, "reboot: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
