#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main tabos_ls_main
#include "apps/coreutils/src/ls/main.c"
#undef main

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "coreutils ls test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void check_output(void (*print)(const ls_entry_t*, size_t), const ls_entry_t* entries, size_t count,
                         const char* expected)
{
    FILE* capture = tmpfile();
    check(capture != NULL, "create output capture");
    FILE* saved = stdout;
    stdout      = capture;
    print(entries, count);
    check(fflush(capture) == 0, "flush output capture");
    stdout = saved;
    check(fseek(capture, 0L, SEEK_SET) == 0, "rewind output capture");
    char output[512]  = {0};
    const size_t size = fread(output, 1U, sizeof(output) - 1U, capture);
    check(!ferror(capture) && fclose(capture) == 0, "read output capture");
    output[size] = '\0';
    check(strcmp(output, expected) == 0, "formatted output");
}

static void print_wide_columns(const ls_entry_t* entries, size_t count)
{
    print_columns(entries, count, 20U);
}

static void print_narrow_columns(const ls_entry_t* entries, size_t count)
{
    print_columns(entries, count, 6U);
}

int main(void)
{
    ls_options_t options;
    char* defaults[] = {"ls"};
    check(parse_options(1, defaults, &options) && !options.long_format && strcmp(options.path, ".") == 0,
          "default options");
    char* long_path[] = {"ls", "-l", "directory"};
    check(parse_options(3, long_path, &options) && options.long_format && strcmp(options.path, "directory") == 0,
          "long option and path");
    char* dashed_path[] = {"ls", "--", "-directory"};
    check(parse_options(3, dashed_path, &options) && !options.long_format && strcmp(options.path, "-directory") == 0,
          "option terminator");
    char* unknown[]      = {"ls", "-x"};
    char* multiple[]     = {"ls", "one", "two"};
    char* dot_multiple[] = {"ls", ".", "two"};
    check(!parse_options(2, unknown, &options) && !parse_options(3, multiple, &options) &&
              !parse_options(3, dot_multiple, &options),
          "reject invalid options");

    ls_entry_t entries[] = {
        {.name = "cc/", .size = 4U, .modified_time = 0, .directory = true, .metadata_valid = true},
        {.name = "a", .size = 123U, .modified_time = 0, .metadata_valid = true},
        {.name = "bbbb", .size = 9U, .modified_time = 0, .metadata_valid = true},
    };
    qsort(entries, 3U, sizeof(entries[0]), compare_entries);
    check(strcmp(entries[0].name, "a") == 0 && strcmp(entries[1].name, "bbbb") == 0 &&
              strcmp(entries[2].name, "cc/") == 0,
          "sort names");
    check_output(print_wide_columns, entries, 3U, "a     bbbb  cc/\n");
    check_output(print_narrow_columns, entries, 3U, "a\nbbbb\ncc/\n");
    check_output(print_long, entries, 3U,
                 "- 123 1970-01-01 00:00 a\n"
                 "-   9 1970-01-01 00:00 bbbb\n"
                 "d   4 1970-01-01 00:00 cc/\n");
    return EXIT_SUCCESS;
}
