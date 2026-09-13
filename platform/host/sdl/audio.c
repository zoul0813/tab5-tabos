#include "internal.h"

#include <tabos/audio.h>
#include <tabos/platform/platform.h>

#include <errno.h>
#include <string.h>

enum {
    HOST_AUDIO_CHUNK_FRAMES = 1024,
    HOST_HEADLESS_TICK_MS   = 10,
};

static SDL_AudioStream* playback_stream;
static SDL_AudioStream* capture_stream;
static platform_audio_render_fn render_callback;
static platform_audio_capture_fn capture_callback;
static platform_audio_error_fn error_callback;
static uint32_t current_sample_rate;
static bool capture_supported;
static bool audio_active;
static SDL_Mutex* headless_mutex;
static SDL_Condition* headless_condition;
static SDL_Thread* headless_thread;
static bool headless_stop_requested;

static int headless_audio_worker(void* unused)
{
    (void) unused;
    int16_t samples[HOST_AUDIO_CHUNK_FRAMES * 2U];
    SDL_LockMutex(headless_mutex);
    const uint32_t sample_rate = current_sample_rate;
    SDL_UnlockMutex(headless_mutex);
    const size_t frames = (sample_rate + 99U) / 100U;

    for (;;) {
        SDL_LockMutex(headless_mutex);
        const bool stopping = headless_stop_requested;
        SDL_UnlockMutex(headless_mutex);
        if (stopping) {
            break;
        }
        render_callback(samples, frames);
        memset(samples, 0, frames * 2U * sizeof(*samples));
        capture_callback(samples, frames, 2U);
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_AUDIO);

        SDL_LockMutex(headless_mutex);
        if (!headless_stop_requested) {
            (void) SDL_WaitConditionTimeout(headless_condition, headless_mutex, HOST_HEADLESS_TICK_MS);
        }
        SDL_UnlockMutex(headless_mutex);
    }
    return 0;
}

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
        case TABOS_AUDIO_SAMPLE_RATE_96000: return true;
        default: return false;
    }
}

bool platform_audio_init(platform_audio_render_fn render, platform_audio_capture_fn capture,
                         platform_audio_error_fn error, platform_audio_info_t* info)
{
    if (render == NULL || capture == NULL || error == NULL || info == NULL) {
        return false;
    }
    const bool headless = host_is_headless();
    if (headless) {
        headless_mutex = SDL_CreateMutex();
        if (headless_mutex == NULL) {
            return false;
        }
        headless_condition = SDL_CreateCondition();
        if (headless_condition == NULL) {
            SDL_DestroyMutex(headless_mutex);
            headless_mutex = NULL;
            return false;
        }
    }
    *info = (platform_audio_info_t) {
        .driver              = headless ? "headless audio" : "SDL3 audio",
        .features            = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
        .routes              = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE | TABOS_AUDIO_ROUTE_MICROPHONE,
        .capture_channels    = 2U,
        .sample_rates        = TABOS_AUDIO_RATES_ALL,
        .default_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE,
        .detected            = true,
        .ready               = true,
    };
    render_callback     = render;
    capture_callback    = capture;
    error_callback      = error;
    current_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE;
    capture_supported   = true;
    if (headless) {
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
    close_streams();
    return true;
}

bool platform_audio_start(uint32_t sample_rate, uint32_t route)
{
    if (audio_active || !sample_rate_supported(sample_rate) ||
        (route != TABOS_AUDIO_ROUTE_SPEAKER && route != TABOS_AUDIO_ROUTE_HEADPHONE &&
         route != TABOS_AUDIO_ROUTE_MICROPHONE)) {
        return false;
    }
    if (host_is_headless()) {
        SDL_LockMutex(headless_mutex);
        current_sample_rate     = sample_rate;
        headless_stop_requested = false;
        audio_active            = true;
        SDL_UnlockMutex(headless_mutex);
        headless_thread = SDL_CreateThread(headless_audio_worker, "tabos-headless-audio", NULL);
        if (headless_thread == NULL) {
            SDL_LockMutex(headless_mutex);
            audio_active = false;
            SDL_UnlockMutex(headless_mutex);
            return false;
        }
        return true;
    }
    if (!open_streams(sample_rate, capture_supported)) {
        return false;
    }
    current_sample_rate = sample_rate;
    audio_active        = true;
    return true;
}

void platform_audio_stop(void)
{
    if (headless_thread != NULL) {
        SDL_LockMutex(headless_mutex);
        headless_stop_requested = true;
        SDL_BroadcastCondition(headless_condition);
        SDL_UnlockMutex(headless_mutex);
        SDL_WaitThread(headless_thread, NULL);
        headless_thread = NULL;
    }
    close_streams();
    audio_active = false;
}

void platform_audio_shutdown(void)
{
    platform_audio_stop();
    if (headless_condition != NULL) {
        SDL_DestroyCondition(headless_condition);
        headless_condition = NULL;
    }
    if (headless_mutex != NULL) {
        SDL_DestroyMutex(headless_mutex);
        headless_mutex = NULL;
    }
    render_callback         = NULL;
    capture_callback        = NULL;
    error_callback          = NULL;
    current_sample_rate     = 0U;
    capture_supported       = false;
    headless_stop_requested = false;
}

bool platform_audio_set_route(uint32_t route)
{
    return route == TABOS_AUDIO_ROUTE_SPEAKER || route == TABOS_AUDIO_ROUTE_HEADPHONE ||
           route == TABOS_AUDIO_ROUTE_MICROPHONE;
}
