#include <tabos/internal/power_config.h>

#include <tabos/filesystem.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

power_policy_t power_config_defaults(void)
{
    return (power_policy_t) {.idle_ms           = 60000U,
                             .screen_off_ms     = 180000U,
                             .panel_off_ms      = 300000U,
                             .suspend_ms        = 600000U,
                             .active_brightness = 75U,
                             .idle_brightness   = 20U,
                             .automatic_suspend = false};
}

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

static bool parse_number(const char* text, uint32_t* number)
{
    if (*text == '\0') {
        return false;
    }
    uint32_t value = 0U;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        const uint32_t digit = (uint32_t) (*text - '0');
        if (value > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    *number = value;
    return true;
}

power_config_result_t power_config_parse(const char* text, size_t length, power_policy_t* policy)
{
    if (length > POWER_CONFIG_FILE_MAX) {
        return POWER_CONFIG_TOO_LARGE;
    }
    if (text == NULL || policy == NULL || memchr(text, '\0', length) != NULL) {
        return POWER_CONFIG_INVALID;
    }
    char buffer[POWER_CONFIG_FILE_MAX + 1U];
    memcpy(buffer, text, length);
    buffer[length]        = '\0';
    power_policy_t parsed = power_config_defaults();
    enum {
        SECTION_ROOT,
        SECTION_DISPLAY,
        SECTION_UNKNOWN
    } section         = SECTION_ROOT;
    bool version_seen = false;
    unsigned int seen = 0U;
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
        line = trim(line);
        if (*line == '\0' || *line == '#' || *line == ';') {
            continue;
        }
        if (*line == '[') {
            const size_t count = strlen(line);
            if (count < 3U || line[count - 1U] != ']') {
                return POWER_CONFIG_INVALID;
            }
            line[count - 1U] = '\0';
            section          = strcmp(trim(line + 1U), "display") == 0 ? SECTION_DISPLAY : SECTION_UNKNOWN;
            continue;
        }
        char* equals = strchr(line, '=');
        if (equals == NULL) {
            return POWER_CONFIG_INVALID;
        }
        *equals           = '\0';
        const char* key   = trim(line);
        const char* value = trim(equals + 1U);
        if (*key == '\0') {
            return POWER_CONFIG_INVALID;
        }
        if (section == SECTION_ROOT && strcmp(key, "version") == 0) {
            if (version_seen || strcmp(value, "1") != 0) {
                return POWER_CONFIG_INVALID;
            }
            version_seen = true;
        } else if (section == SECTION_DISPLAY) {
            static const char* const keys[] = {"dim_seconds", "backlight_off_seconds", "panel_off_seconds",
                                               "normal_brightness", "dim_brightness"};
            for (size_t index = 0U; index < sizeof(keys) / sizeof(keys[0]); ++index) {
                if (strcmp(key, keys[index]) != 0) {
                    continue;
                }
                uint32_t number;
                const unsigned int bit = 1U << index;
                if ((seen & bit) != 0U || !parse_number(value, &number)) {
                    return POWER_CONFIG_INVALID;
                }
                seen |= bit;
                if (index >= 3U && (number > 100U || (index == 3U && number == 0U))) {
                    return POWER_CONFIG_INVALID;
                }
                switch (index) {
                    case 0U: parsed.idle_ms = (uint64_t) number * 1000U; break;
                    case 1U: parsed.screen_off_ms = (uint64_t) number * 1000U; break;
                    case 2U: parsed.panel_off_ms = (uint64_t) number * 1000U; break;
                    case 3U: parsed.active_brightness = (uint8_t) number; break;
                    case 4U: parsed.idle_brightness = (uint8_t) number; break;
                }
                break;
            }
        }
    }
    if (!version_seen || parsed.idle_ms == 0U ||
        (parsed.screen_off_ms != 0U && parsed.screen_off_ms < parsed.idle_ms) ||
        (parsed.panel_off_ms != 0U && (parsed.screen_off_ms == 0U || parsed.panel_off_ms < parsed.screen_off_ms))) {
        return POWER_CONFIG_INVALID;
    }
    *policy = parsed;
    return POWER_CONFIG_OK;
}

power_config_result_t power_config_load(power_policy_t* policy)
{
    if (policy == NULL) {
        return POWER_CONFIG_INVALID;
    }
    const tabos_fd_t file = tabos_fs_open(POWER_CONFIG_PATH, TABOS_O_RDONLY, 0U);
    if (file < 0) {
        const int error = *tabos_errno_location();
        if (error == TABOS_ENOENT) {
            return POWER_CONFIG_NOT_FOUND;
        }
        return error == TABOS_ENODEV ? POWER_CONFIG_UNAVAILABLE : POWER_CONFIG_IO_ERROR;
    }
    char* buffer = malloc(POWER_CONFIG_FILE_MAX + 1U);
    if (buffer == NULL) {
        (void) tabos_fs_close(file);
        return POWER_CONFIG_IO_ERROR;
    }
    size_t used                  = 0U;
    power_config_result_t result = POWER_CONFIG_OK;
    while (used < POWER_CONFIG_FILE_MAX + 1U) {
        const tabos_ssize_t count = tabos_fs_read(file, buffer + used, POWER_CONFIG_FILE_MAX + 1U - used);
        if (count < 0) {
            result = POWER_CONFIG_IO_ERROR;
            break;
        }
        if (count == 0) {
            break;
        }
        used += (size_t) count;
    }
    if (tabos_fs_close(file) != 0) {
        result = POWER_CONFIG_IO_ERROR;
    }
    if (result == POWER_CONFIG_OK) {
        result = power_config_parse(buffer, used, policy);
    }
    free(buffer);
    return result;
}

const char* power_config_result_name(power_config_result_t result)
{
    switch (result) {
        case POWER_CONFIG_OK: return "OK";
        case POWER_CONFIG_NOT_FOUND: return "not found";
        case POWER_CONFIG_UNAVAILABLE: return "storage unavailable";
        case POWER_CONFIG_INVALID: return "invalid configuration";
        case POWER_CONFIG_TOO_LARGE: return "configuration too large";
        case POWER_CONFIG_IO_ERROR: return "I/O error";
    }
    return "unknown error";
}
