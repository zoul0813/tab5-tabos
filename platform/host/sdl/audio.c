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

bool platform_audio_init(platform_audio_render_fn render, platform_audio_capture_fn capture,
                         platform_audio_error_fn error, platform_audio_info_t* info)
{
    if (render == NULL || capture == NULL || error == NULL || info == NULL) {
        return false;
    }
    *info = (platform_audio_info_t) {
        .driver           = "SDL3 audio",
        .features         = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
        .routes           = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE | TABOS_AUDIO_ROUTE_MICROPHONE,
        .capture_channels = 2U,
        .detected         = true,
        .ready            = true,
    };
    render_callback  = render;
    capture_callback = capture;
    error_callback   = error;
    if (host_is_headless()) {
        return true;
    }
    const SDL_AudioSpec specification = {
        .format   = SDL_AUDIO_S16LE,
        .channels = 2,
        .freq     = TABOS_AUDIO_SAMPLE_RATE,
    };
    playback_stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, playback_needed, NULL);
    if (playback_stream == NULL || !SDL_ResumeAudioStreamDevice(playback_stream)) {
        info->ready = false;
        info->error = EIO;
        platform_audio_shutdown();
        return false;
    }
    capture_stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &specification, capture_available, NULL);
    if (capture_stream == NULL || !SDL_ResumeAudioStreamDevice(capture_stream)) {
        info->features         &= ~((uint32_t) TABOS_AUDIO_FEATURE_CAPTURE);
        info->routes           &= ~((uint32_t) TABOS_AUDIO_ROUTE_MICROPHONE);
        info->capture_channels  = 0U;
        if (capture_stream != NULL) {
            SDL_DestroyAudioStream(capture_stream);
            capture_stream = NULL;
        }
    }
    return true;
}

void platform_audio_shutdown(void)
{
    if (capture_stream != NULL) {
        SDL_DestroyAudioStream(capture_stream);
        capture_stream = NULL;
    }
    if (playback_stream != NULL) {
        SDL_DestroyAudioStream(playback_stream);
        playback_stream = NULL;
    }
    render_callback  = NULL;
    capture_callback = NULL;
    error_callback   = NULL;
}

bool platform_audio_set_route(uint32_t route)
{
    return route == TABOS_AUDIO_ROUTE_SPEAKER || route == TABOS_AUDIO_ROUTE_HEADPHONE ||
           route == TABOS_AUDIO_ROUTE_MICROPHONE;
}
