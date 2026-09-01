#ifndef TABOS_AUDIO_H
#define TABOS_AUDIO_H

#include <stdint.h>

enum {
    TABOS_AUDIO_SAMPLE_RATE_8000  = 8000,
    TABOS_AUDIO_SAMPLE_RATE_11025 = 11025,
    TABOS_AUDIO_SAMPLE_RATE_12000 = 12000,
    TABOS_AUDIO_SAMPLE_RATE_16000 = 16000,
    TABOS_AUDIO_SAMPLE_RATE_22050 = 22050,
    TABOS_AUDIO_SAMPLE_RATE_24000 = 24000,
    TABOS_AUDIO_SAMPLE_RATE_32000 = 32000,
    TABOS_AUDIO_SAMPLE_RATE_44100 = 44100,
    TABOS_AUDIO_SAMPLE_RATE_48000 = 48000,
    TABOS_AUDIO_SAMPLE_RATE_88200 = 88200,
    TABOS_AUDIO_SAMPLE_RATE_96000 = 96000,
    TABOS_AUDIO_DEFAULT_SAMPLE_RATE = TABOS_AUDIO_SAMPLE_RATE_44100,
    TABOS_AUDIO_SAMPLE_RATE         = TABOS_AUDIO_DEFAULT_SAMPLE_RATE,
    TABOS_AUDIO_SAMPLE_BITS      = 16,
    TABOS_AUDIO_VOLUME_MAX       = 1000,
    TABOS_AUDIO_IO_MAX           = 16384,
    TABOS_AUDIO_ROUTE_SPEAKER    = 1U << 0U,
    TABOS_AUDIO_ROUTE_HEADPHONE  = 1U << 1U,
    TABOS_AUDIO_ROUTE_MICROPHONE = 1U << 2U,
    TABOS_AUDIO_FEATURE_PLAYBACK = 1U << 0U,
    TABOS_AUDIO_FEATURE_CAPTURE  = 1U << 1U,
    TABOS_AUDIO_FEATURE_AEC      = 1U << 2U,
    TABOS_AUDIO_RATE_8000         = 1U << 0U,
    TABOS_AUDIO_RATE_11025        = 1U << 1U,
    TABOS_AUDIO_RATE_12000        = 1U << 2U,
    TABOS_AUDIO_RATE_16000        = 1U << 3U,
    TABOS_AUDIO_RATE_22050        = 1U << 4U,
    TABOS_AUDIO_RATE_24000        = 1U << 5U,
    TABOS_AUDIO_RATE_32000        = 1U << 6U,
    TABOS_AUDIO_RATE_44100        = 1U << 7U,
    TABOS_AUDIO_RATE_48000        = 1U << 8U,
    TABOS_AUDIO_RATE_88200        = 1U << 9U,
    TABOS_AUDIO_RATE_96000        = 1U << 10U,
    TABOS_AUDIO_RATES_ALL         = (1U << 11U) - 1U,
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
        uint32_t sample_rates;
        uint32_t default_sample_rate;
} tabos_audio_info_t;

typedef struct {
        tabos_audio_direction_t direction;
        uint32_t channels;
        uint32_t route;
        uint32_t sample_rate;
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
