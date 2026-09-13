#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <tabos/filesystem.h>
#include <tabos/tty.h>

typedef struct {
        char* name;
        uint64_t size;
        int64_t modified_time;
        bool directory;
        bool metadata_valid;
} ls_entry_t;

typedef struct {
        const char* path;
        bool long_format;
} ls_options_t;

static void usage(FILE* stream)
{
    fprintf(stream, "Usage: ls [-l] [path]\n");
}

static bool parse_options(int argc, char** argv, ls_options_t* options)
{
    *options           = (ls_options_t) {.path = "."};
    bool options_ended = false;
    bool path_seen     = false;
    for (int index = 1; index < argc; ++index) {
        if (!options_ended && strcmp(argv[index], "--") == 0) {
            options_ended = true;
        } else if (!options_ended && strcmp(argv[index], "-l") == 0) {
            options->long_format = true;
        } else if (!options_ended && argv[index][0] == '-') {
            return false;
        } else if (path_seen) {
            return false;
        } else {
            options->path = argv[index];
            path_seen     = true;
        }
    }
    return true;
}

static int compare_entries(const void* left, const void* right)
{
    const ls_entry_t* left_entry  = left;
    const ls_entry_t* right_entry = right;
    return strcmp(left_entry->name, right_entry->name);
}

static void free_entries(ls_entry_t* entries, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        free(entries[index].name);
    }
    free(entries);
}

static bool entry_path(const char* directory, const char* name, char path[TABOS_FS_PATH_MAX])
{
    const size_t directory_length = strlen(directory);
    const bool separator          = directory_length > 0U && directory[directory_length - 1U] != '/';
    const int length              = snprintf(path, TABOS_FS_PATH_MAX, "%s%s%s", directory, separator ? "/" : "", name);
    return length > 0 && (size_t) length < TABOS_FS_PATH_MAX;
}

static int load_entries(const char* path, bool load_metadata, ls_entry_t** result, size_t* result_count)
{
    DIR* directory = opendir(path);
    if (directory == NULL) {
        fprintf(stderr, "ls: cannot open %s (errno %d)\n", path, errno);
        return 1;
    }

    ls_entry_t* entries = NULL;
    size_t count        = 0U;
    size_t capacity     = 0U;
    int status          = 0;
    errno               = 0;
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (count == capacity) {
            const size_t new_capacity = capacity == 0U ? 16U : capacity * 2U;
            if (new_capacity < capacity || new_capacity > SIZE_MAX / sizeof(*entries)) {
                errno  = ENOMEM;
                status = 1;
                break;
            }
            ls_entry_t* grown = realloc(entries, new_capacity * sizeof(*entries));
            if (grown == NULL) {
                status = 1;
                break;
            }
            entries  = grown;
            capacity = new_capacity;
        }
        const size_t name_length = strlen(entry->d_name);
        char* name               = malloc(name_length + 2U);
        if (name == NULL) {
            status = 1;
            break;
        }
        memcpy(name, entry->d_name, name_length + 1U);
        entries[count] = (ls_entry_t) {
            .name = name,
        };
        bool needs_stat = load_metadata;
#ifdef DT_DIR
        entries[count].directory = entry->d_type == DT_DIR;
#ifdef DT_UNKNOWN
        needs_stat = needs_stat || entry->d_type == DT_UNKNOWN;
#endif
#else
        needs_stat = true;
#endif
        if (needs_stat) {
            char full_path[TABOS_FS_PATH_MAX];
            struct stat metadata;
            if (!entry_path(path, entry->d_name, full_path)) {
                fprintf(stderr, "ls: cannot stat %s (errno %d)\n", entry->d_name, ENAMETOOLONG);
                status = 1;
            } else if (stat(full_path, &metadata) != 0) {
                fprintf(stderr, "ls: cannot stat %s (errno %d)\n", entry->d_name, errno);
                status = 1;
            } else {
                entries[count].directory = S_ISDIR(metadata.st_mode);
                if (load_metadata) {
                    entries[count].size           = (uint64_t) metadata.st_size;
                    entries[count].modified_time  = metadata.st_mtime;
                    entries[count].metadata_valid = true;
                }
            }
        }
        if (entries[count].directory) {
            name[name_length]      = '/';
            name[name_length + 1U] = '\0';
        }
        ++count;
        errno = 0;
    }
    if (errno != 0 && status == 0) {
        fprintf(stderr, "ls: read %s failed (errno %d)\n", path, errno);
        status = 1;
    } else if (status != 0 && errno == ENOMEM) {
        fprintf(stderr, "ls: out of memory\n");
    }
    if (closedir(directory) != 0) {
        fprintf(stderr, "ls: close failed (errno %d)\n", errno);
        status = 1;
    }
    if (count > 1U) {
        qsort(entries, count, sizeof(*entries), compare_entries);
    }
    *result       = entries;
    *result_count = count;
    return status;
}

static size_t terminal_width(void)
{
    tabos_tty_size_t size = {.columns = 80U};
    if (ioctl(STDOUT_FILENO, TABOS_TTY_GET_SIZE, &size) != 0 || size.columns == 0U) {
        return 80U;
    }
    return size.columns;
}

static void print_columns(const ls_entry_t* entries, size_t count, size_t width)
{
    size_t name_width = 0U;
    for (size_t index = 0U; index < count; ++index) {
        const size_t length = strlen(entries[index].name);
        if (length > name_width) {
            name_width = length;
        }
    }
    const size_t cell_width = name_width <= SIZE_MAX - 2U ? name_width + 2U : name_width;
    size_t columns          = cell_width == 0U ? 1U : (width + 2U) / cell_width;
    if (columns == 0U) {
        columns = 1U;
    }
    for (size_t index = 0U; index < count; ++index) {
        const size_t length = strlen(entries[index].name);
        fputs(entries[index].name, stdout);
        const bool end_of_row = (index + 1U) % columns == 0U || index + 1U == count;
        if (end_of_row) {
            fputc('\n', stdout);
        } else {
            for (size_t padding = length; padding < cell_width; ++padding) {
                fputc(' ', stdout);
            }
        }
    }
}

static size_t decimal_width(uint64_t value)
{
    size_t width = 1U;
    while (value >= 10U) {
        value /= 10U;
        ++width;
    }
    return width;
}

static void print_long(const ls_entry_t* entries, size_t count)
{
    size_t size_width = 1U;
    for (size_t index = 0U; index < count; ++index) {
        if (entries[index].metadata_valid) {
            const size_t width = decimal_width(entries[index].size);
            if (width > size_width) {
                size_width = width;
            }
        }
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!entries[index].metadata_valid) {
            printf("? %*s ---------------- %s\n", (int) size_width, "?", entries[index].name);
            continue;
        }
        char timestamp[17]         = "----------------";
        const time_t modified_time = (time_t) entries[index].modified_time;
        const struct tm* calendar  = gmtime(&modified_time);
        if (calendar != NULL) {
            (void) strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M", calendar);
        }
        printf("%c %*llu %s %s\n", entries[index].directory ? 'd' : '-', (int) size_width,
               (unsigned long long) entries[index].size, timestamp, entries[index].name);
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }
    ls_options_t options;
    if (!parse_options(argc, argv, &options)) {
        usage(stderr);
        return 2;
    }

    ls_entry_t* entries = NULL;
    size_t count        = 0U;
    const int status    = load_entries(options.path, options.long_format, &entries, &count);
    if (entries != NULL || status == 0) {
        if (options.long_format) {
            print_long(entries, count);
        } else {
            print_columns(entries, count, terminal_width());
        }
    }
    free_entries(entries, count);
    return status;
}
