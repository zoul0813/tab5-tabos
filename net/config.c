#include <tabos/internal/network_config.h>

#include <tabos/filesystem.h>

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char NETWORK_CONFIG_PATH[]      = "T:/etc/wifi.conf";
static const char NETWORK_CONFIG_TEMP_PATH[] = "T:/etc/.wifi.conf.tmp";
static atomic_flag config_write_lock         = ATOMIC_FLAG_INIT;

static char* trim(char* text)
{
    while (isspace((unsigned char) *text) != 0) {
        ++text;
    }
    char* end = text + strlen(text);
    while (end > text && isspace((unsigned char) end[-1]) != 0) {
        --end;
    }
    *end = '\0';
    return text;
}

static bool parse_quoted(const char* source, char* destination, size_t capacity)
{
    if (source[0] != '"' || capacity == 0U) {
        return false;
    }
    size_t used = 0U;
    for (size_t index = 1U; source[index] != '\0'; ++index) {
        char value = source[index];
        if (value == '"') {
            if (source[index + 1U] != '\0') {
                return false;
            }
            destination[used] = '\0';
            return true;
        }
        if (value == '\\') {
            ++index;
            value = source[index];
            if (value != '\\' && value != '"') {
                return false;
            }
        }
        if (used + 1U >= capacity) {
            return false;
        }
        destination[used++] = value;
    }
    return false;
}

static bool parse_boolean(const char* value, bool* parsed)
{
    if (strcmp(value, "true") == 0) {
        *parsed = true;
        return true;
    }
    if (strcmp(value, "false") == 0) {
        *parsed = false;
        return true;
    }
    return false;
}

static bool valid_hostname(const char* hostname)
{
    const size_t length = strlen(hostname);
    if (length == 0U || length > NETWORK_CONFIG_NAME_MAX || hostname[0] == '-' || hostname[length - 1U] == '-') {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        const unsigned char character = (unsigned char) hostname[index];
        if (isalnum(character) == 0 && character != '-') {
            return false;
        }
    }
    return true;
}

network_config_result_t network_config_parse(const char* text, size_t length, network_config_t* config)
{
    if (text == NULL || config == NULL || length > NETWORK_CONFIG_FILE_MAX) {
        return length > NETWORK_CONFIG_FILE_MAX ? NETWORK_CONFIG_TOO_LARGE : NETWORK_CONFIG_INVALID;
    }
    char buffer[NETWORK_CONFIG_FILE_MAX + 1U];
    memcpy(buffer, text, length);
    buffer[length] = '\0';

    network_config_t parsed = {
        .name         = NETWORK_CONFIG_DEFAULT_NAME,
        .auto_connect = true,
    };
    bool version_seen = false;
    bool wifi_section = false;
    bool ssid_seen    = false;
    char* cursor      = buffer;
    while (*cursor != '\0') {
        char* line = cursor;
        char* end  = strchr(cursor, '\n');
        if (end != NULL) {
            *end   = '\0';
            cursor = end + 1U;
        } else {
            cursor += strlen(cursor);
        }
        const size_t line_length = strlen(line);
        if (line_length > 0U && line[line_length - 1U] == '\r') {
            line[line_length - 1U] = '\0';
        }
        line = trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line[0] == '[') {
            const size_t section_length = strlen(line);
            if (section_length < 3U || line[section_length - 1U] != ']') {
                return NETWORK_CONFIG_INVALID;
            }
            line[section_length - 1U] = '\0';
            wifi_section              = strcmp(trim(line + 1U), "wifi") == 0;
            continue;
        }
        char* equals = strchr(line, '=');
        if (equals == NULL) {
            return NETWORK_CONFIG_INVALID;
        }
        *equals           = '\0';
        const char* key   = trim(line);
        const char* value = trim(equals + 1U);
        if (!wifi_section && strcmp(key, "version") == 0) {
            if (strcmp(value, "1") != 0) {
                return NETWORK_CONFIG_INVALID;
            }
            version_seen = true;
        } else if (wifi_section && strcmp(key, "ssid") == 0) {
            if (!parse_quoted(value, parsed.ssid, sizeof(parsed.ssid)) || parsed.ssid[0] == '\0') {
                return NETWORK_CONFIG_INVALID;
            }
            ssid_seen = true;
        } else if (wifi_section && strcmp(key, "password") == 0) {
            if (!parse_quoted(value, parsed.password, sizeof(parsed.password))) {
                return NETWORK_CONFIG_INVALID;
            }
        } else if (wifi_section && strcmp(key, "name") == 0) {
            if (!parse_quoted(value, parsed.name, sizeof(parsed.name)) || !valid_hostname(parsed.name)) {
                return NETWORK_CONFIG_INVALID;
            }
        } else if (wifi_section && strcmp(key, "auto_connect") == 0 && !parse_boolean(value, &parsed.auto_connect)) {
            return NETWORK_CONFIG_INVALID;
        }
    }
    if (!version_seen || !ssid_seen) {
        return NETWORK_CONFIG_INVALID;
    }
    *config = parsed;
    return NETWORK_CONFIG_OK;
}

network_config_result_t network_config_load(network_config_t* config)
{
    if (config == NULL) {
        return NETWORK_CONFIG_INVALID;
    }
    const tabos_fd_t file = tabos_fs_open(NETWORK_CONFIG_PATH, TABOS_O_RDONLY, 0U);
    if (file < 0) {
        const int error = *tabos_errno_location();
        if (error == TABOS_ENOENT) {
            return NETWORK_CONFIG_NOT_FOUND;
        }
        if (error == TABOS_ENODEV) {
            return NETWORK_CONFIG_UNAVAILABLE;
        }
        return NETWORK_CONFIG_IO_ERROR;
    }
    char* buffer = malloc(NETWORK_CONFIG_FILE_MAX + 1U);
    if (buffer == NULL) {
        (void) tabos_fs_close(file);
        return NETWORK_CONFIG_IO_ERROR;
    }
    size_t used = 0U;
    while (used < NETWORK_CONFIG_FILE_MAX + 1U) {
        const tabos_ssize_t count = tabos_fs_read(file, buffer + used, NETWORK_CONFIG_FILE_MAX + 1U - used);
        if (count < 0) {
            (void) tabos_fs_close(file);
            free(buffer);
            return NETWORK_CONFIG_IO_ERROR;
        }
        if (count == 0) {
            break;
        }
        used += (size_t) count;
    }
    if (tabos_fs_close(file) != 0) {
        free(buffer);
        return NETWORK_CONFIG_IO_ERROR;
    }
    if (used > NETWORK_CONFIG_FILE_MAX) {
        free(buffer);
        return NETWORK_CONFIG_TOO_LARGE;
    }
    const network_config_result_t result = network_config_parse(buffer, used, config);
    free(buffer);
    return result;
}

static bool append_bytes(char* output, size_t capacity, size_t* used, const char* bytes, size_t count)
{
    if (count > capacity - *used) {
        return false;
    }
    memcpy(output + *used, bytes, count);
    *used += count;
    return true;
}

static bool append_quoted(char* output, size_t capacity, size_t* used, const char* value)
{
    if (!append_bytes(output, capacity, used, "\"", 1U)) {
        return false;
    }
    for (size_t index = 0U; value[index] != '\0'; ++index) {
        if ((value[index] == '\\' || value[index] == '"') && !append_bytes(output, capacity, used, "\\", 1U)) {
            return false;
        }
        if (!append_bytes(output, capacity, used, value + index, 1U)) {
            return false;
        }
    }
    return append_bytes(output, capacity, used, "\"\n", 2U);
}

static bool is_owned_line(const char* line, size_t length, bool wifi_section)
{
    char copy[NETWORK_CONFIG_FILE_MAX + 1U];
    if (length > NETWORK_CONFIG_FILE_MAX) {
        return false;
    }
    memcpy(copy, line, length);
    copy[length] = '\0';
    char* text   = trim(copy);
    char* equals = strchr(text, '=');
    if (equals == NULL) {
        return false;
    }
    *equals         = '\0';
    const char* key = trim(text);
    if (!wifi_section) {
        return strcmp(key, "version") == 0;
    }
    return strcmp(key, "ssid") == 0 || strcmp(key, "password") == 0 || strcmp(key, "name") == 0 ||
           strcmp(key, "auto_connect") == 0;
}

network_config_result_t network_config_format_update(const char* existing, size_t existing_length,
                                                     const network_config_t* config, char* output,
                                                     size_t output_capacity, size_t* output_length)
{
    if ((existing == NULL && existing_length != 0U) || config == NULL || output == NULL || output_length == NULL ||
        config->ssid[0] == '\0' || strnlen(config->ssid, sizeof(config->ssid)) > NETWORK_CONFIG_SSID_MAX ||
        strnlen(config->password, sizeof(config->password)) > NETWORK_CONFIG_PASSWORD_MAX ||
        (config->name[0] != '\0' && !valid_hostname(config->name)) || existing_length > NETWORK_CONFIG_FILE_MAX) {
        return NETWORK_CONFIG_INVALID;
    }
    const char* name = config->name[0] != '\0' ? config->name : NETWORK_CONFIG_DEFAULT_NAME;
    size_t used      = 0U;
    if (!append_bytes(output, output_capacity, &used, "version=1\n", 10U)) {
        return NETWORK_CONFIG_TOO_LARGE;
    }
    bool wifi_section = false;
    size_t offset     = 0U;
    while (offset < existing_length) {
        const char* line            = existing + offset;
        const char* newline         = memchr(line, '\n', existing_length - offset);
        const size_t content_length = newline != NULL ? (size_t) (newline - line) : existing_length - offset;
        const size_t full_length    = content_length + (newline != NULL ? 1U : 0U);

        char section[NETWORK_CONFIG_FILE_MAX + 1U];
        if (content_length > NETWORK_CONFIG_FILE_MAX) {
            return NETWORK_CONFIG_TOO_LARGE;
        }
        memcpy(section, line, content_length);
        section[content_length]     = '\0';
        char* trimmed               = trim(section);
        const size_t trimmed_length = strlen(trimmed);
        if (trimmed_length >= 3U && trimmed[0] == '[' && trimmed[trimmed_length - 1U] == ']') {
            trimmed[trimmed_length - 1U] = '\0';
            wifi_section                 = strcmp(trim(trimmed + 1U), "wifi") == 0;
        }
        if (!is_owned_line(line, content_length, wifi_section) &&
            !append_bytes(output, output_capacity, &used, line, full_length)) {
            return NETWORK_CONFIG_TOO_LARGE;
        }
        offset += full_length;
    }
    if (used > 0U && output[used - 1U] != '\n' && !append_bytes(output, output_capacity, &used, "\n", 1U)) {
        return NETWORK_CONFIG_TOO_LARGE;
    }
    if (!append_bytes(output, output_capacity, &used, "\n[wifi]\nssid=", 13U) ||
        !append_quoted(output, output_capacity, &used, config->ssid) ||
        !append_bytes(output, output_capacity, &used, "password=", 9U) ||
        !append_quoted(output, output_capacity, &used, config->password) ||
        !append_bytes(output, output_capacity, &used, "name=", 5U) ||
        !append_quoted(output, output_capacity, &used, name)) {
        return NETWORK_CONFIG_TOO_LARGE;
    }
    const char* auto_connect = config->auto_connect ? "auto_connect=true\n" : "auto_connect=false\n";
    if (!append_bytes(output, output_capacity, &used, auto_connect, strlen(auto_connect))) {
        return NETWORK_CONFIG_TOO_LARGE;
    }
    if (used >= output_capacity) {
        return NETWORK_CONFIG_TOO_LARGE;
    }
    output[used]   = '\0';
    *output_length = used;
    return NETWORK_CONFIG_OK;
}

static network_config_result_t read_existing(char* buffer, size_t* length)
{
    const tabos_fd_t file = tabos_fs_open(NETWORK_CONFIG_PATH, TABOS_O_RDONLY, 0U);
    if (file < 0) {
        return *tabos_errno_location() == TABOS_ENOENT ? NETWORK_CONFIG_NOT_FOUND : NETWORK_CONFIG_IO_ERROR;
    }
    size_t used = 0U;
    while (used < NETWORK_CONFIG_FILE_MAX + 1U) {
        const tabos_ssize_t count = tabos_fs_read(file, buffer + used, NETWORK_CONFIG_FILE_MAX + 1U - used);
        if (count <= 0) {
            if (count < 0) {
                (void) tabos_fs_close(file);
                return NETWORK_CONFIG_IO_ERROR;
            }
            break;
        }
        used += (size_t) count;
    }
    if (tabos_fs_close(file) != 0) {
        return NETWORK_CONFIG_IO_ERROR;
    }
    if (used > NETWORK_CONFIG_FILE_MAX) {
        return NETWORK_CONFIG_TOO_LARGE;
    }
    *length = used;
    return NETWORK_CONFIG_OK;
}

network_config_result_t network_config_save(const network_config_t* config)
{
    if (config == NULL) {
        return NETWORK_CONFIG_INVALID;
    }
    while (atomic_flag_test_and_set_explicit(&config_write_lock, memory_order_acquire)) {}

    char existing[NETWORK_CONFIG_FILE_MAX + 1U];
    size_t existing_length         = 0U;
    network_config_result_t result = read_existing(existing, &existing_length);
    if (result == NETWORK_CONFIG_NOT_FOUND) {
        existing_length = 0U;
    } else if (result != NETWORK_CONFIG_OK) {
        atomic_flag_clear_explicit(&config_write_lock, memory_order_release);
        return result;
    }

    char output[NETWORK_CONFIG_FILE_MAX + 1U];
    size_t output_length = 0U;
    result = network_config_format_update(existing, existing_length, config, output, sizeof(output), &output_length);
    if (result != NETWORK_CONFIG_OK) {
        atomic_flag_clear_explicit(&config_write_lock, memory_order_release);
        return result;
    }
    if (tabos_fs_mkdir("T:/etc", 0755U) != 0 && *tabos_errno_location() != TABOS_EEXIST) {
        atomic_flag_clear_explicit(&config_write_lock, memory_order_release);
        return *tabos_errno_location() == TABOS_ENODEV ? NETWORK_CONFIG_UNAVAILABLE : NETWORK_CONFIG_IO_ERROR;
    }
    const tabos_fd_t file =
        tabos_fs_open(NETWORK_CONFIG_TEMP_PATH, TABOS_O_WRONLY | TABOS_O_CREAT | TABOS_O_TRUNC, 0600U);
    if (file < 0) {
        atomic_flag_clear_explicit(&config_write_lock, memory_order_release);
        return NETWORK_CONFIG_IO_ERROR;
    }
    const tabos_ssize_t written = tabos_fs_write(file, output, output_length);
    const int close_result      = tabos_fs_close(file);
    if (written != (tabos_ssize_t) output_length || close_result != 0 ||
        tabos_fs_rename(NETWORK_CONFIG_TEMP_PATH, NETWORK_CONFIG_PATH) != 0) {
        (void) tabos_fs_unlink(NETWORK_CONFIG_TEMP_PATH);
        atomic_flag_clear_explicit(&config_write_lock, memory_order_release);
        return NETWORK_CONFIG_IO_ERROR;
    }
    atomic_flag_clear_explicit(&config_write_lock, memory_order_release);
    return NETWORK_CONFIG_OK;
}

const char* network_config_result_name(network_config_result_t result)
{
    switch (result) {
        case NETWORK_CONFIG_OK: return "OK";
        case NETWORK_CONFIG_NOT_FOUND: return "not found";
        case NETWORK_CONFIG_UNAVAILABLE: return "storage unavailable";
        case NETWORK_CONFIG_INVALID: return "invalid configuration";
        case NETWORK_CONFIG_TOO_LARGE: return "configuration too large";
        case NETWORK_CONFIG_IO_ERROR: return "I/O error";
    }
    return "unknown error";
}
