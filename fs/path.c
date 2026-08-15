#include <tabos/internal/filesystem.h>

#include <tabos/filesystem.h>

#include <string.h>

static bool append_component(char *output, size_t output_size, size_t *length,
                             const char *component, size_t component_length)
{
    if (component_length > TABOS_FS_NAME_MAX) return false;
    const size_t separator = *length > 1U ? 1U : 0U;
    if (*length + separator + component_length >= output_size) return false;
    if (separator != 0U) output[(*length)++] = '/';
    memcpy(output + *length, component, component_length);
    *length += component_length;
    output[*length] = '\0';
    return true;
}

static void remove_component(char *output, size_t *length)
{
    if (*length <= 1U) return;
    while (*length > 1U && output[*length - 1U] != '/') --*length;
    if (*length > 1U) --*length;
    output[*length] = '\0';
}

static bool consume_path(const char *path, char *output, size_t output_size, size_t *length)
{
    const char *cursor = path;
    while (*cursor != '\0') {
        while (*cursor == '/') ++cursor;
        if (*cursor == '\0') break;
        const char *component = cursor;
        while (*cursor != '\0' && *cursor != '/') ++cursor;
        const size_t component_length = (size_t)(cursor - component);
        if (component_length == 1U && component[0] == '.') continue;
        if (component_length == 2U && component[0] == '.' && component[1] == '.') {
            remove_component(output, length);
            continue;
        }
        if (!append_component(output, output_size, length, component, component_length)) {
            return false;
        }
    }
    return true;
}

bool tab_fs_normalize_path(const char *path, const char *working_directory,
                           char *output, size_t output_size)
{
    if (path == NULL || working_directory == NULL || output == NULL || output_size < 2U ||
        path[0] == '\0' || working_directory[0] != '/') {
        return false;
    }
    size_t length = 1U;
    output[0] = '/';
    output[1] = '\0';
    if (path[0] != '/' && !consume_path(working_directory, output, output_size, &length)) {
        return false;
    }
    return consume_path(path, output, output_size, &length);
}
