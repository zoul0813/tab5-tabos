#include "internal.h"

#include <tabos/audio.h>
#include <tabos/platform/platform.h>

#include <errno.h>

enum {
    HOST_AUDIO_CHUNK_FRAMES = 1024,
};

static SDL_AudioStream* playback_stream;
static SDL_AudioStream* capture_stream;
static platform_audio_render_fn render_callback;
static platform_audio_capture_fn capture_callback;
static platform_audio_error_fn error_callback;
static uint32_t current_sample_rate;
static bool capture_supported;

static void SDLCALL playback_needed(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    (void) userdata;
    (void) total_amount;
    int16_t samples[HOST_AUDIO_CHUNK_FRAMES * 2U];
    int remaining = additional_amount;
    while (remaining > 0) {
        int bytes = remaining;
        if (bytes > (int) sizeof(samples)) {
            bytes = (int) sizeof(samples);
        }
        bytes -= bytes % (int) (2U * sizeof(int16_t));
        if (bytes == 0) {
            break;
        }
        const size_t frames = (size_t) bytes / (2U * sizeof(int16_t));
        render_callback(samples, frames);
        if (!SDL_PutAudioStreamData(stream, samples, bytes)) {
            error_callback(EIO);
            break;
        }
        remaining -= bytes;
    }
}

static void SDLCALL capture_available(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    (void) userdata;
    (void) additional_amount;
    int16_t samples[HOST_AUDIO_CHUNK_FRAMES * 2U];
    int remaining = total_amount;
    while (remaining > 0) {
        int bytes = remaining;
        if (bytes > (int) sizeof(samples)) {
            bytes = (int) sizeof(samples);
        }
        bytes -= bytes % (int) (2U * sizeof(int16_t));
        if (bytes == 0) {
            break;
        }
        const int received = SDL_GetAudioStreamData(stream, samples, bytes);
        if (received <= 0) {
            if (received < 0) {
                error_callback(EIO);
            }
            break;
        }
        capture_callback(samples, (size_t) received / (2U * sizeof(int16_t)), 2U);
        remaining -= received;
    }
}

static void close_streams(void)
{
    if (capture_stream != NULL) {
        SDL_DestroyAudioStream(capture_stream);
        capture_stream = NULL;
    }
    if (playback_stream != NULL) {
        SDL_DestroyAudioStream(playback_stream);
        playback_stream = NULL;
    }
}

static bool open_streams(uint32_t sample_rate, bool open_capture)
{
    const SDL_AudioSpec specification = {
        .format   = SDL_AUDIO_S16LE,
        .channels = 2,
        .freq     = (int) sample_rate,
    };
    playback_stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, playback_needed, NULL);
    if (playback_stream == NULL || !SDL_ResumeAudioStreamDevice(playback_stream)) {
        close_streams();
        return false;
    }
    if (!open_capture) {
        return true;
    }
    capture_stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &specification, capture_available, NULL);
    if (capture_stream == NULL || !SDL_ResumeAudioStreamDevice(capture_stream)) {
        close_streams();
        return false;
    }
    return true;
}

static bool sample_rate_supported(uint32_t sample_rate)
{
    switch (sample_rate) {
        case TABOS_AUDIO_SAMPLE_RATE_8000:
        case TABOS_AUDIO_SAMPLE_RATE_11025:
        case TABOS_AUDIO_SAMPLE_RATE_12000:
        case TABOS_AUDIO_SAMPLE_RATE_16000:
        case TABOS_AUDIO_SAMPLE_RATE_22050:
        case TABOS_AUDIO_SAMPLE_RATE_24000:
        case TABOS_AUDIO_SAMPLE_RATE_32000:
        case TABOS_AUDIO_SAMPLE_RATE_44100:
        case TABOS_AUDIO_SAMPLE_RATE_48000:
        case TABOS_AUDIO_SAMPLE_RATE_88200:
        case TABOS_AUDIO_SAMPLE_RATE_96000:
            return true;
        default:
            return false;
    }
}

bool platform_audio_init(platform_audio_render_fn render, platform_audio_capture_fn capture,
                         platform_audio_error_fn error, platform_audio_info_t* info)
{
    if (render == NULL || capture == NULL || error == NULL || info == NULL) {
        return false;
    }
    *info = (platform_audio_info_t) {
        .driver              = "SDL3 audio",
        .features            = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
        .routes              = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE | TABOS_AUDIO_ROUTE_MICROPHONE,
        .capture_channels    = 2U,
        .sample_rates        = TABOS_AUDIO_RATES_ALL,
        .default_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE,
        .detected            = true,
        .ready               = true,
    };
    render_callback  = render;
    capture_callback = capture;
    error_callback   = error;
    current_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE;
    capture_supported   = true;
    if (host_is_headless()) {
        return true;
    }
    if (!open_streams(current_sample_rate, true)) {
        capture_supported = false;
        if (!open_streams(current_sample_rate, false)) {
            info->ready = false;
            info->error = EIO;
            platform_audio_shutdown();
            return false;
        }
        info->features         &= ~((uint32_t) TABOS_AUDIO_FEATURE_CAPTURE);
        info->routes           &= ~((uint32_t) TABOS_AUDIO_ROUTE_MICROPHONE);
        info->capture_channels  = 0U;
    }
    return true;
}

bool platform_audio_set_sample_rate(uint32_t sample_rate)
{
    if (!sample_rate_supported(sample_rate)) {
        return false;
    }
    if (sample_rate == current_sample_rate) {
        return true;
    }
    if (host_is_headless()) {
        current_sample_rate = sample_rate;
        return true;
    }
    const uint32_t previous_sample_rate = current_sample_rate;
    close_streams();
    if (!open_streams(sample_rate, capture_supported)) {
        (void) open_streams(previous_sample_rate, capture_supported);
        return false;
    }
    current_sample_rate = sample_rate;
    return true;
}

void platform_audio_shutdown(void)
{
    close_streams();
    render_callback      = NULL;
    capture_callback     = NULL;
    error_callback       = NULL;
    current_sample_rate  = 0U;
    capture_supported    = false;
}

bool platform_audio_set_route(uint32_t route)
{
    return route == TABOS_AUDIO_ROUTE_SPEAKER || route == TABOS_AUDIO_ROUTE_HEADPHONE ||
           route == TABOS_AUDIO_ROUTE_MICROPHONE;
}
