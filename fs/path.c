#include <tabos/internal/filesystem.h>

#include <tabos/filesystem.h>

#include <string.h>

static bool append_component(char *output, size_t output_size, size_t *length,
                             const char *component, size_t component_length)
{
    if (component_length > TABOS_FS_NAME_MAX) return false;
    const size_t separator = *length > 3U ? 1U : 0U;
    if (*length + separator + component_length >= output_size) return false;
    if (separator != 0U) output[(*length)++] = '/';
    memcpy(output + *length, component, component_length);
    *length += component_length;
    output[*length] = '\0';
    return true;
}

static void remove_component(char *output, size_t *length)
{
    if (*length <= 3U) return;
    while (*length > 3U && output[*length - 1U] != '/') --*length;
    if (*length > 3U) --*length;
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

bool filesystem_normalize_path(const char *path, const char *working_directory,
                           char *output, size_t output_size)
{
    if (path == NULL || working_directory == NULL || output == NULL || output_size < 4U ||
        path[0] == '\0' || working_directory[0] < 'A' || working_directory[0] > 'Z' ||
        working_directory[1] != ':' || working_directory[2] != '/') {
        return false;
    }

    const bool qualified = ((path[0] >= 'A' && path[0] <= 'Z') ||
                            (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
    const char drive = qualified
        ? (char)(path[0] >= 'a' ? path[0] - ('a' - 'A') : path[0])
        : working_directory[0];
    const char *source = qualified ? path + 2U : path;
    if (qualified && source[0] != '\0' && source[0] != '/') return false;

    size_t length = 3U;
    output[0] = drive;
    output[1] = ':';
    output[2] = '/';
    output[3] = '\0';
    if (!qualified && source[0] != '/' &&
        !consume_path(working_directory + 2U, output, output_size, &length)) {
        return false;
    }
    return consume_path(source, output, output_size, &length);
}
