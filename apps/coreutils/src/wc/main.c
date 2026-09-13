#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
        uint32_t lines;
        uint32_t words;
        uint32_t bytes;
} WcCount;

static bool is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static bool wc(FILE* fp, WcCount* count)
{
    *count       = (WcCount) {0};
    bool in_word = false;
    int c;

    while ((c = fgetc(fp)) != EOF) {
        count->bytes++;

        if (c == '\n') {
            count->lines++;
        }

        if (is_space(c)) {
            in_word = false;
        } else if (!in_word) {
            count->words++;
            in_word = true;
        }
    }

    return !ferror(fp);
}

static void print_count(WcCount count, const char* name)
{
    printf("%8lu %8lu %8lu", (unsigned long) count.lines, (unsigned long) count.words, (unsigned long) count.bytes);

    if (name) {
        printf(" %s", name);
    }

    putchar('\n');
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: wc FILE...\n");
        return 1;
    }

    WcCount total = {0};
    int files     = 0;
    int result    = 0;

    for (int i = 1; i < argc; i++) {
        FILE* fp = fopen(argv[i], "rb");

        if (!fp) {
            fprintf(stderr, "wc: %s: %s\n", argv[i], strerror(errno));
            result = 1;
            continue;
        }

        WcCount count;
        if (!wc(fp, &count)) {
            const int error = errno != 0 ? errno : EIO;
            fprintf(stderr, "wc: %s: %s\n", argv[i], strerror(error));
            result = 1;
        }
        if (fclose(fp) != 0) {
            fprintf(stderr, "wc: %s: %s\n", argv[i], strerror(errno));
            result = 1;
        }

        print_count(count, argv[i]);

        total.lines += count.lines;
        total.words += count.words;
        total.bytes += count.bytes;

        files++;
    }

    if (files > 1) {
        print_count(total, "total");
    }

    return result;
}
