#include <tabos/internal/power_config.h>
#include <tabos/filesystem.h>

#include <assert.h>
#include <string.h>

static const char* file_text;
static size_t file_length;
static size_t file_offset;
static int file_error;
static bool read_failure;
static bool close_failure;
static unsigned int closes;

int* tabos_errno_location(void)
{
    return &file_error;
}

tabos_fd_t tabos_fs_open(const char* path, int flags, uint32_t mode)
{
    assert(strcmp(path, POWER_CONFIG_PATH) == 0 && flags == TABOS_O_RDONLY && mode == 0U);
    file_offset = 0U;
    return file_error == 0 ? 1 : -1;
}

tabos_ssize_t tabos_fs_read(tabos_fd_t fd, void* buffer, size_t count)
{
    assert(fd == 1);
    if (read_failure) {
        return -1;
    }
    size_t length = file_length - file_offset;
    if (length > count) {
        length = count;
    }
    if (length > 7U) {
        length = 7U; /* Force the loader to handle short reads. */
    }
    memcpy(buffer, file_text + file_offset, length);
    file_offset += length;
    return (tabos_ssize_t) length;
}

int tabos_fs_close(tabos_fd_t fd)
{
    assert(fd == 1);
    ++closes;
    return close_failure ? -1 : 0;
}

static void expect_invalid(const char* text)
{
    power_policy_t policy    = power_config_defaults();
    policy.active_brightness = 42U;
    assert(power_config_parse(text, strlen(text), &policy) == POWER_CONFIG_INVALID);
    assert(policy.active_brightness == 42U);
}

int main(void)
{
    power_policy_t policy;
    const char* valid = " # comment\r\nversion = 1\r\n; comment\r\n[ display ]\r\n"
                        "dim_seconds=10\r\nbacklight_off_seconds=120\r\npanel_off_seconds=300\r\n"
                        "normal_brightness=55\r\ndim_brightness=12\r\nfuture=hello\r\n[future]\r\nversion=2";
    assert(power_config_parse(valid, strlen(valid), &policy) == POWER_CONFIG_OK);
    assert(policy.idle_ms == 10000U && policy.screen_off_ms == 120000U && policy.panel_off_ms == 300000U);
    assert(policy.active_brightness == 55U && policy.idle_brightness == 12U && !policy.automatic_suspend);
    assert(power_config_parse("version=1", 9U, &policy) == POWER_CONFIG_OK);
    assert(policy.idle_ms == 60000U && policy.screen_off_ms == 180000U && policy.panel_off_ms == 300000U);
    assert(policy.active_brightness == 75U && policy.idle_brightness == 20U);
    expect_invalid("");
    expect_invalid("[display]\ndim_seconds=5");
    expect_invalid("[future]\nversion=1");
    expect_invalid("version=2");
    expect_invalid("version=1\nversion=1");
    expect_invalid("version=1\n[display]\ndim_seconds=1\ndim_seconds=2");
    expect_invalid("version=1\n[display");
    expect_invalid("version=1\n[display]\nno equals");
    expect_invalid("version=1\n[display]\n=5");
    const char* bad_values[] = {"-1", "+1", "1.5", "1s", "\"60\"", "", "4294967296", "99999999999999999999"};
    for (size_t index = 0U; index < sizeof(bad_values) / sizeof(bad_values[0]); ++index) {
        char text[128] = "version=1\n[display]\ndim_seconds=";
        strcat(text, bad_values[index]);
        expect_invalid(text);
    }
    expect_invalid("version=1\n[display]\nnormal_brightness=0");
    expect_invalid("version=1\n[display]\nnormal_brightness=101");
    expect_invalid("version=1\n[display]\ndim_brightness=101");
    expect_invalid("version=1\n[display]\ndim_seconds=0");
    expect_invalid("version=1\n[display]\nbacklight_off_seconds=59");
    expect_invalid("version=1\n[display]\npanel_off_seconds=179");
    expect_invalid("version=1\n[display]\nbacklight_off_seconds=0");
    const char* disabled = "version=1\n[display]\nbacklight_off_seconds=0\npanel_off_seconds=0\n"
                           "normal_brightness=1\ndim_brightness=0";
    assert(power_config_parse(disabled, strlen(disabled), &policy) == POWER_CONFIG_OK);
    assert(policy.screen_off_ms == 0U && policy.panel_off_ms == 0U && policy.idle_brightness == 0U);
    const char* maximum = "version=1\n[display]\ndim_seconds=4294967295\nbacklight_off_seconds=4294967295\n"
                          "panel_off_seconds=4294967295\nnormal_brightness=100\ndim_brightness=100";
    assert(power_config_parse(maximum, strlen(maximum), &policy) == POWER_CONFIG_OK);
    assert(policy.panel_off_ms == UINT64_C(4294967295000));
    const char embedded[] = "version=1\0[display]\nnormal_brightness=0";
    assert(power_config_parse(embedded, sizeof(embedded) - 1U, &policy) == POWER_CONFIG_INVALID);
    assert(power_config_parse(NULL, 0U, &policy) == POWER_CONFIG_INVALID);
    assert(power_config_parse(valid, strlen(valid), NULL) == POWER_CONFIG_INVALID);
    char large[POWER_CONFIG_FILE_MAX + 1U];
    memset(large, ' ', sizeof(large));
    memcpy(large, "version=1\n#", 11U);
    assert(power_config_parse(large, POWER_CONFIG_FILE_MAX, &policy) == POWER_CONFIG_OK);
    assert(power_config_parse(large, sizeof(large), &policy) == POWER_CONFIG_TOO_LARGE);

    file_text   = valid;
    file_length = strlen(valid);
    assert(power_config_load(&policy) == POWER_CONFIG_OK && policy.active_brightness == 55U && closes == 1U);
    read_failure = true;
    assert(power_config_load(&policy) == POWER_CONFIG_IO_ERROR && closes == 2U);
    read_failure  = false;
    close_failure = true;
    assert(power_config_load(&policy) == POWER_CONFIG_IO_ERROR && closes == 3U);
    close_failure = false;
    file_text     = large;
    file_length   = sizeof(large);
    assert(power_config_load(&policy) == POWER_CONFIG_TOO_LARGE && closes == 4U);
    assert(policy.active_brightness == 55U);
    file_error = TABOS_ENOENT;
    assert(power_config_load(&policy) == POWER_CONFIG_NOT_FOUND);
    file_error = TABOS_ENODEV;
    assert(power_config_load(&policy) == POWER_CONFIG_UNAVAILABLE);
    file_error = TABOS_EIO;
    assert(power_config_load(&policy) == POWER_CONFIG_IO_ERROR);
    assert(closes == 4U && policy.active_brightness == 55U);
    assert(power_config_load(NULL) == POWER_CONFIG_INVALID);
    return 0;
}
