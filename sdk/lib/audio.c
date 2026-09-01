#include <tabos/audio.h>

#include <tabos/internal/elf_api.h>

#include <errno.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int api_result(int result)
{
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

int tabos_audio_get_info(tabos_audio_info_t* info)
{
    if (info == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_info == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_info(info));
}

tabos_audio_stream_t tabos_audio_open(const tabos_audio_config_t* config)
{
    if (config == NULL) {
        errno = EINVAL;
        return TABOS_AUDIO_STREAM_INVALID;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_open == NULL) {
        errno = ENOSYS;
        return TABOS_AUDIO_STREAM_INVALID;
    }
    const int result = tabos_runtime_api->audio_open(config);
    if (result < 0) {
        errno = -result;
        return TABOS_AUDIO_STREAM_INVALID;
    }
    return result;
}

int tabos_audio_close(tabos_audio_stream_t stream)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_close == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_close(stream));
}

int tabos_audio_flush(tabos_audio_stream_t stream)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_flush == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_flush(stream));
}

int tabos_audio_write(tabos_audio_stream_t stream, const void* pcm, uint32_t bytes)
{
    if (pcm == NULL || bytes == 0U) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_write == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_write(stream, pcm, bytes));
}

int tabos_audio_read(tabos_audio_stream_t stream, void* pcm, uint32_t capacity)
{
    if (pcm == NULL || capacity == 0U) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_read == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_read(stream, pcm, capacity));
}

int tabos_audio_set_volume(tabos_audio_stream_t stream, uint32_t volume)
{
    if (volume > TABOS_AUDIO_VOLUME_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_set_volume == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_set_volume(stream, volume));
}

int tabos_audio_set_route(tabos_audio_stream_t stream, uint32_t route)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_set_route == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_set_route(stream, route));
}

int tabos_audio_get_status(tabos_audio_stream_t stream, tabos_audio_status_t* status)
{
    if (status == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (tabos_runtime_api == NULL || tabos_runtime_api->audio_status == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return api_result(tabos_runtime_api->audio_status(stream, status));
}
