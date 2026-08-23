#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void print_hex_byte(unsigned char value)
{
    static const char digits[] = "0123456789abcdef";
    char text[3]               = {digits[value >> 4U], digits[value & 0x0fU], '\0'};
    printf("%s", text);
}

static void print_offset(unsigned int offset)
{
    static const char digits[] = "0123456789abcdef";
    char text[9];
    for (int index = 7; index >= 0; --index) {
        text[index]   = digits[offset & 0x0fU];
        offset      >>= 4U;
    }
    text[8] = '\0';
    printf("%s  ", text);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: hexdump file\n");
        return 1;
    }

    const int descriptor = open(argv[1], O_RDONLY);
    if (descriptor < 0) {
        fprintf(stderr, "hexdump: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    unsigned char buffer[16];
    unsigned int offset = 0U;
    for (;;) {
        const ssize_t count = read(descriptor, buffer, sizeof(buffer));
        if (count < 0) {
            fprintf(stderr, "hexdump: read failed: %s\n", strerror(errno));
            (void) close(descriptor);
            return 1;
        }
        if (count == 0) {
            break;
        }

        print_offset(offset);
        for (ssize_t index = 0; index < 16; ++index) {
            if (index < count) {
                print_hex_byte(buffer[index]);
                printf("%s", index == 7 ? "  " : " ");
            } else {
                printf("  ");
                printf("%s", index == 7 ? "  " : " ");
            }
        }
        printf(" | ");
        for (ssize_t index = 0; index < count; ++index) {
            const unsigned char value = buffer[index];
            putchar(value >= 32U && value <= 126U ? (int) value : '.');
        }
        putchar('\n');
        offset += (unsigned int) count;
    }

    return close(descriptor) == 0 ? 0 : 1;
}
