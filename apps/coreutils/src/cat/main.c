#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    BUFFER_SIZE = 1024,
};

static int write_stream(FILE* input, const char* name)
{
    unsigned char buffer[BUFFER_SIZE];
    for (;;) {
        const size_t count = fread(buffer, 1U, sizeof(buffer), input);
        if (count > 0U && fwrite(buffer, 1U, count, stdout) != count) {
            fprintf(stderr, "cat: write: %s\n", strerror(errno));
            return 1;
        }
        if (count < sizeof(buffer)) {
            if (ferror(input)) {
                fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
                return 1;
            }
            return 0;
        }
    }
}

int main(int argc, char** argv)
{
    int result = 0;
    if (argc == 1) {
        fprintf(stderr, "usage: cat FILE...\n");
        return 2;
    }
    for (int index = 1; index < argc; ++index) {
        FILE* input = fopen(argv[index], "rb");
        if (input == NULL) {
            fprintf(stderr, "cat: %s: %s\n", argv[index], strerror(errno));
            result = 1;
            continue;
        }
        if (write_stream(input, argv[index]) != 0) {
            result = 1;
        }
        if (fclose(input) != 0) {
            fprintf(stderr, "cat: %s: %s\n", argv[index], strerror(errno));
            result = 1;
        }
    }
    return result;
}
