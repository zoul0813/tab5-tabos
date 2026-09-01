#include <tabos/audio.h>
#include <tabos/filesystem.h>
#include <tabos/wait.h>

#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <stdint.h>

const tabos_elf_api_t* tabos_runtime_api;

static int info_call(tabos_audio_info_t* info)
{
    *info = (tabos_audio_info_t) {
        .features         = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
        .routes           = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_MICROPHONE,
        .capture_channels = 2U,
        .sample_rates     = TABOS_AUDIO_RATES_ALL,
        .default_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE,
    };
    return 0;
}

static int open_call(const tabos_audio_config_t* config)
{
    return config->channels == 2U && config->sample_rate == TABOS_AUDIO_SAMPLE_RATE_11025 ? 17 : -TABOS_EINVAL;
}

static int close_call(int stream)
{
    return stream == 17 ? 0 : -TABOS_EBADF;
}

static int flush_call(int stream)
{
    return stream == 17 ? 0 : -TABOS_EBADF;
}

static int write_call(int stream, const void* pcm, uint32_t bytes)
{
    return stream == 17 && pcm != NULL ? (int) bytes : -TABOS_EBADF;
}

static int read_call(int stream, void* pcm, uint32_t capacity)
{
    (void) pcm;
    (void) capacity;
    return stream == 17 ? -TABOS_EAGAIN : -TABOS_EBADF;
}

static int value_call(int stream, uint32_t value)
{
    return stream == 17 && value != 0U ? 0 : -TABOS_EINVAL;
}

static int status_call(int stream, tabos_audio_status_t* status)
{
    if (stream != 17) {
        return -TABOS_EBADF;
    }
    *status = (tabos_audio_status_t) {.buffer_capacity = 32768U};
    return 0;
}

static int wait_source_call(int stream)
{
    return stream == 17 ? 23 : -TABOS_EBADF;
}

int main(void)
{
    const tabos_elf_api_t api = {
        .abi_version       = TABOS_ELF_API_VERSION,
        .audio_info        = info_call,
        .audio_open        = open_call,
        .audio_close       = close_call,
        .audio_write       = write_call,
        .audio_read        = read_call,
        .audio_set_volume  = value_call,
        .audio_set_route   = value_call,
        .audio_status      = status_call,
        .audio_wait_source = wait_source_call,
        .audio_flush       = flush_call,
    };
    tabos_runtime_api = &api;
    tabos_audio_info_t info;
    const tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 2U,
        .route     = TABOS_AUDIO_ROUTE_SPEAKER,
        .sample_rate = TABOS_AUDIO_SAMPLE_RATE_11025,
    };
    int16_t pcm[2] = {0};
    tabos_audio_status_t status;
    const tabos_audio_stream_t stream = tabos_audio_open(&config);
    if (tabos_audio_get_info(&info) != 0 || info.capture_channels != 2U ||
        info.default_sample_rate != TABOS_AUDIO_DEFAULT_SAMPLE_RATE || info.sample_rates != TABOS_AUDIO_RATES_ALL ||
        stream != 17) {
        return 1;
    }
    if (tabos_audio_write(stream, pcm, sizeof(pcm)) != (int) sizeof(pcm) ||
        tabos_audio_read(stream, pcm, sizeof(pcm)) != -1 || errno != TABOS_EAGAIN) {
        return 2;
    }
    if (tabos_audio_set_volume(stream, 500U) != 0 || tabos_audio_set_route(stream, TABOS_AUDIO_ROUTE_SPEAKER) != 0 ||
        tabos_audio_get_status(stream, &status) != 0 || status.buffer_capacity != 32768U ||
        tabos_audio_wait_source(stream) != 23 || tabos_audio_flush(stream) != 0 || tabos_audio_close(stream) != 0) {
        return 3;
    }
    tabos_runtime_api = NULL;
    if (tabos_audio_get_info(&info) != -1 || errno != ENOSYS || tabos_audio_open(&config) != -1 || errno != ENOSYS) {
        return 4;
    }
    return 0;
}
