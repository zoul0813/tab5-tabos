#ifndef TABOS_AUDIO_H
#define TABOS_AUDIO_H

#include <stdint.h>

enum {
    TABOS_AUDIO_SAMPLE_RATE      = 48000,
    TABOS_AUDIO_SAMPLE_BITS      = 16,
    TABOS_AUDIO_VOLUME_MAX       = 1000,
    TABOS_AUDIO_IO_MAX           = 16384,
    TABOS_AUDIO_ROUTE_SPEAKER    = 1U << 0U,
    TABOS_AUDIO_ROUTE_HEADPHONE  = 1U << 1U,
    TABOS_AUDIO_ROUTE_MICROPHONE = 1U << 2U,
    TABOS_AUDIO_FEATURE_PLAYBACK = 1U << 0U,
    TABOS_AUDIO_FEATURE_CAPTURE  = 1U << 1U,
    TABOS_AUDIO_FEATURE_AEC      = 1U << 2U,
};

#define TABOS_AUDIO_STREAM_INVALID (-1)

typedef int32_t tabos_audio_stream_t;

typedef enum {
    TABOS_AUDIO_PLAYBACK = 0,
    TABOS_AUDIO_CAPTURE,
} tabos_audio_direction_t;

typedef struct {
        uint32_t features;
        uint32_t routes;
        uint32_t capture_channels;
} tabos_audio_info_t;

typedef struct {
        tabos_audio_direction_t direction;
        uint32_t channels;
        uint32_t route;
} tabos_audio_config_t;

typedef struct {
        uint32_t buffered_bytes;
        uint32_t buffer_capacity;
        uint32_t underruns;
        uint32_t overruns;
} tabos_audio_status_t;

int tabos_audio_get_info(tabos_audio_info_t* info);
tabos_audio_stream_t tabos_audio_open(const tabos_audio_config_t* config);
int tabos_audio_close(tabos_audio_stream_t stream);
int tabos_audio_flush(tabos_audio_stream_t stream);
int tabos_audio_write(tabos_audio_stream_t stream, const void* pcm, uint32_t bytes);
int tabos_audio_read(tabos_audio_stream_t stream, void* pcm, uint32_t capacity);
int tabos_audio_set_volume(tabos_audio_stream_t stream, uint32_t volume);
int tabos_audio_set_route(tabos_audio_stream_t stream, uint32_t route);
int tabos_audio_get_status(tabos_audio_stream_t stream, tabos_audio_status_t* status);

#endif
