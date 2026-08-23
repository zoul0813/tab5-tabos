#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: cp source destination\n");
        return 1;
    }
    int source = open(argv[1], O_RDONLY);
    if (source < 0) {
        fprintf(stderr, "cp: %s: errno %d\n", argv[1], errno);
        return 1;
    }
    int destination = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (destination < 0) {
        fprintf(stderr, "cp: %s: errno %d\n", argv[2], errno);
        close(source);
        return 1;
    }
    char buffer[1024];
    int status = 0;
    ssize_t count;
    while ((count = read(source, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;
        while (written < count) {
            const ssize_t result = write(destination, buffer + written, (size_t) (count - written));
            if (result <= 0) {
                status = 1;
                break;
            }
            written += result;
        }
        if (status != 0) {
            break;
        }
    }
    if (count < 0) {
        status = 1;
    }
    if (close(source) != 0 || close(destination) != 0) {
        status = 1;
    }
    if (status != 0) {
        fprintf(stderr, "cp: copy failed (errno %d)\n", errno);
    }
    return status;
}
