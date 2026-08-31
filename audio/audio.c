#include <tabos/internal/audio.h>

#include <tabos/filesystem.h>
#include <tabos/wait.h>
#include <tabos/platform/platform.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    AUDIO_STREAM_CAPACITY       = 8,
    AUDIO_STREAM_INDEX_BITS     = 3,
    AUDIO_STREAM_INDEX_MASK     = AUDIO_STREAM_CAPACITY - 1,
    AUDIO_STREAM_GENERATION_MAX = INT32_MAX >> AUDIO_STREAM_INDEX_BITS,
    AUDIO_RING_CAPACITY         = 32 * 1024,
};

_Static_assert((1U << AUDIO_STREAM_INDEX_BITS) == AUDIO_STREAM_CAPACITY, "audio stream index bits must match capacity");

typedef struct {
        const void* owner;
        uint8_t* ring;
        size_t head;
        size_t count;
        uint32_t generation;
        uint32_t channels;
        uint32_t route;
        uint32_t volume;
        uint32_t underruns;
        uint32_t overruns;
        tabos_audio_direction_t direction;
        bool open;
        bool faulted;
} audio_stream_t;

static audio_stream_t streams[AUDIO_STREAM_CAPACITY];
static platform_mutex_t* audio_mutex;
static platform_audio_info_t platform_info;
static bool initialized;

static uint32_t next_generation(uint32_t generation)
{
    ++generation;
    if (generation == 0U || generation > AUDIO_STREAM_GENERATION_MAX) {
        generation = 1U;
    }
    return generation;
}

static tabos_audio_stream_t stream_handle(size_t index, uint32_t generation)
{
    return (tabos_audio_stream_t) ((generation << AUDIO_STREAM_INDEX_BITS) | (uint32_t) index);
}

static audio_stream_t* owned_stream(const void* owner, tabos_audio_stream_t handle)
{
    if (owner == NULL || handle <= 0) {
        return NULL;
    }
    const uint32_t value      = (uint32_t) handle;
    const uint32_t index      = value & AUDIO_STREAM_INDEX_MASK;
    const uint32_t generation = value >> AUDIO_STREAM_INDEX_BITS;
    audio_stream_t* stream    = &streams[index];
    if (!stream->open || stream->owner != owner || stream->generation != generation) {
        return NULL;
    }
    return stream;
}

static size_t frame_bytes(const audio_stream_t* stream)
{
    return stream->channels * sizeof(int16_t);
}

static uint8_t ring_pop_byte(audio_stream_t* stream)
{
    const uint8_t value = stream->ring[stream->head];
    stream->head        = (stream->head + 1U) % AUDIO_RING_CAPACITY;
    --stream->count;
    return value;
}

static void ring_push_byte(audio_stream_t* stream, uint8_t value)
{
    const size_t tail  = (stream->head + stream->count) % AUDIO_RING_CAPACITY;
    stream->ring[tail] = value;
    ++stream->count;
}

static int16_t ring_pop_sample(audio_stream_t* stream)
{
    const uint16_t low  = ring_pop_byte(stream);
    const uint16_t high = ring_pop_byte(stream);
    return (int16_t) (low | (uint16_t) (high << 8U));
}

static void ring_push_sample(audio_stream_t* stream, int16_t sample)
{
    const uint16_t value = (uint16_t) sample;
    ring_push_byte(stream, (uint8_t) value);
    ring_push_byte(stream, (uint8_t) (value >> 8U));
}

static int16_t saturate(int32_t sample)
{
    if (sample > INT16_MAX) {
        return INT16_MAX;
    }
    if (sample < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) sample;
}

bool audio_service_init(void)
{
    if (initialized) {
        return true;
    }
    memset(streams, 0, sizeof(streams));
    platform_info = (platform_audio_info_t) {0};
    audio_mutex   = platform_mutex_create();
    if (audio_mutex == NULL) {
        return false;
    }
    initialized = true;
    (void) platform_audio_init(audio_service_render, audio_service_capture, audio_service_error, &platform_info);
    return true;
}

void audio_service_shutdown(void)
{
    if (!initialized) {
        return;
    }
    platform_audio_shutdown();
    platform_mutex_lock(audio_mutex);
    for (size_t index = 0U; index < AUDIO_STREAM_CAPACITY; ++index) {
        free(streams[index].ring);
        streams[index].ring = NULL;
        streams[index].open = false;
    }
    platform_mutex_unlock(audio_mutex);
    platform_mutex_destroy(audio_mutex);
    audio_mutex   = NULL;
    platform_info = (platform_audio_info_t) {0};
    initialized   = false;
}

bool audio_service_info(tabos_audio_info_t* info, const char** driver, int* error)
{
    if (!initialized) {
        return false;
    }
    platform_mutex_lock(audio_mutex);
    if (info != NULL) {
        *info = (tabos_audio_info_t) {
            .features         = platform_info.features,
            .routes           = platform_info.routes,
            .capture_channels = platform_info.capture_channels,
        };
    }
    if (driver != NULL) {
        *driver = platform_info.driver;
    }
    if (error != NULL) {
        *error = platform_info.error;
    }
    const bool detected = platform_info.detected;
    platform_mutex_unlock(audio_mutex);
    return detected;
}

static bool config_valid(const tabos_audio_config_t* config)
{
    if (config == NULL || !platform_info.ready || config->channels == 0U ||
        config->channels > TABOS_AUDIO_IO_MAX / sizeof(int16_t) || (config->route & (config->route - 1U)) != 0U ||
        (config->route & platform_info.routes) == 0U) {
        return false;
    }
    if (config->direction == TABOS_AUDIO_PLAYBACK) {
        return config->channels <= 2U && (platform_info.features & TABOS_AUDIO_FEATURE_PLAYBACK) != 0U &&
               (config->route == TABOS_AUDIO_ROUTE_SPEAKER || config->route == TABOS_AUDIO_ROUTE_HEADPHONE);
    }
    if (config->direction == TABOS_AUDIO_CAPTURE) {
        return (platform_info.features & TABOS_AUDIO_FEATURE_CAPTURE) != 0U &&
               config->route == TABOS_AUDIO_ROUTE_MICROPHONE && config->channels <= platform_info.capture_channels;
    }
    return false;
}

tabos_audio_stream_t audio_service_open(const void* owner, const tabos_audio_config_t* config)
{
    if (!initialized || owner == NULL) {
        return -TABOS_EINVAL;
    }
    uint8_t* ring = malloc(AUDIO_RING_CAPACITY);
    if (ring == NULL) {
        return -TABOS_ENOMEM;
    }
    platform_mutex_lock(audio_mutex);
    if (!platform_info.ready) {
        platform_mutex_unlock(audio_mutex);
        free(ring);
        return -TABOS_ENODEV;
    }
    if (!config_valid(config)) {
        platform_mutex_unlock(audio_mutex);
        free(ring);
        return -TABOS_EINVAL;
    }
    for (size_t index = 0U; index < AUDIO_STREAM_CAPACITY; ++index) {
        if (streams[index].open) {
            continue;
        }
        const uint32_t generation = next_generation(streams[index].generation);
        streams[index]            = (audio_stream_t) {
                       .owner      = owner,
                       .ring       = ring,
                       .generation = generation,
                       .channels   = config->channels,
                       .route      = config->route,
                       .volume     = TABOS_AUDIO_VOLUME_MAX,
                       .direction  = config->direction,
                       .open       = true,
        };
        const tabos_audio_stream_t handle = stream_handle(index, generation);
        platform_mutex_unlock(audio_mutex);
        if (!platform_audio_set_route(config->route)) {
            (void) audio_service_close(owner, handle);
            return -TABOS_EIO;
        }
        return handle;
    }
    platform_mutex_unlock(audio_mutex);
    free(ring);
    return -TABOS_EMFILE;
}

int audio_service_close(const void* owner, tabos_audio_stream_t handle)
{
    if (!initialized) {
        return -TABOS_EBADF;
    }
    platform_mutex_lock(audio_mutex);
    audio_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EBADF;
    }
    uint8_t* ring             = stream->ring;
    const uint32_t generation = stream->generation;
    *stream                   = (audio_stream_t) {.generation = generation};
    platform_mutex_unlock(audio_mutex);
    free(ring);
    return 0;
}

void audio_service_close_owner(const void* owner)
{
    if (!initialized || owner == NULL) {
        return;
    }
    for (size_t index = 0U; index < AUDIO_STREAM_CAPACITY; ++index) {
        platform_mutex_lock(audio_mutex);
        const bool close = streams[index].open && streams[index].owner == owner;
        const tabos_audio_stream_t handle =
            close ? stream_handle(index, streams[index].generation) : TABOS_AUDIO_STREAM_INVALID;
        platform_mutex_unlock(audio_mutex);
        if (close) {
            (void) audio_service_close(owner, handle);
        }
    }
}

int audio_service_write(const void* owner, tabos_audio_stream_t handle, const void* pcm, uint32_t bytes)
{
    if (!initialized || pcm == NULL || bytes == 0U || bytes > TABOS_AUDIO_IO_MAX) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(audio_mutex);
    audio_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL || stream->direction != TABOS_AUDIO_PLAYBACK) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EBADF;
    }
    if (stream->faulted) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EIO;
    }
    const size_t frame = frame_bytes(stream);
    if (bytes % frame != 0U) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EINVAL;
    }
    size_t copied  = AUDIO_RING_CAPACITY - stream->count;
    copied        -= copied % frame;
    if (copied > bytes) {
        copied = bytes;
    }
    if (copied == 0U) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EAGAIN;
    }
    const uint8_t* source = pcm;
    for (size_t index = 0U; index < copied; ++index) {
        ring_push_byte(stream, source[index]);
    }
    platform_mutex_unlock(audio_mutex);
    return (int) copied;
}

int audio_service_read(const void* owner, tabos_audio_stream_t handle, void* pcm, uint32_t capacity)
{
    if (!initialized || pcm == NULL || capacity == 0U || capacity > TABOS_AUDIO_IO_MAX) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(audio_mutex);
    audio_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL || stream->direction != TABOS_AUDIO_CAPTURE) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EBADF;
    }
    if (stream->faulted) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EIO;
    }
    const size_t frame = frame_bytes(stream);
    if (capacity % frame != 0U) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EINVAL;
    }
    size_t copied  = stream->count < capacity ? stream->count : capacity;
    copied        -= copied % frame;
    if (copied == 0U) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EAGAIN;
    }
    uint8_t* destination = pcm;
    for (size_t index = 0U; index < copied; ++index) {
        destination[index] = ring_pop_byte(stream);
    }
    platform_mutex_unlock(audio_mutex);
    return (int) copied;
}

int audio_service_set_volume(const void* owner, tabos_audio_stream_t handle, uint32_t volume)
{
    if (!initialized || volume > TABOS_AUDIO_VOLUME_MAX) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(audio_mutex);
    audio_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL || stream->direction != TABOS_AUDIO_PLAYBACK) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EBADF;
    }
    stream->volume = volume;
    platform_mutex_unlock(audio_mutex);
    return 0;
}

int audio_service_set_route(const void* owner, tabos_audio_stream_t handle, uint32_t route)
{
    if (!initialized || (route & (route - 1U)) != 0U || (route & platform_info.routes) == 0U) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(audio_mutex);
    audio_stream_t* stream = owned_stream(owner, handle);
    const bool valid =
        stream != NULL && ((stream->direction == TABOS_AUDIO_PLAYBACK &&
                            (route == TABOS_AUDIO_ROUTE_SPEAKER || route == TABOS_AUDIO_ROUTE_HEADPHONE)) ||
                           (stream->direction == TABOS_AUDIO_CAPTURE && route == TABOS_AUDIO_ROUTE_MICROPHONE));
    if (!valid) {
        platform_mutex_unlock(audio_mutex);
        return stream == NULL ? -TABOS_EBADF : -TABOS_EINVAL;
    }
    platform_mutex_unlock(audio_mutex);
    if (!platform_audio_set_route(route)) {
        return -TABOS_EIO;
    }
    platform_mutex_lock(audio_mutex);
    stream = owned_stream(owner, handle);
    if (stream != NULL) {
        stream->route = route;
    }
    platform_mutex_unlock(audio_mutex);
    return stream != NULL ? 0 : -TABOS_EBADF;
}

int audio_service_get_status(const void* owner, tabos_audio_stream_t handle, tabos_audio_status_t* status)
{
    if (!initialized || status == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(audio_mutex);
    const audio_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EBADF;
    }
    *status = (tabos_audio_status_t) {
        .buffered_bytes  = (uint32_t) stream->count,
        .buffer_capacity = AUDIO_RING_CAPACITY,
        .underruns       = stream->underruns,
        .overruns        = stream->overruns,
    };
    platform_mutex_unlock(audio_mutex);
    return 0;
}

int audio_service_poll(const void* owner, tabos_audio_stream_t handle, uint32_t requested_events,
                       uint32_t* returned_events)
{
    if (!initialized || returned_events == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(audio_mutex);
    const audio_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(audio_mutex);
        return -TABOS_EBADF;
    }
    uint32_t ready     = 0U;
    const size_t frame = frame_bytes(stream);
    if (stream->faulted) {
        ready |= TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP;
    }
    if (stream->direction == TABOS_AUDIO_PLAYBACK && AUDIO_RING_CAPACITY - stream->count >= frame) {
        ready |= TABOS_WAIT_WRITABLE;
    }
    if (stream->direction == TABOS_AUDIO_CAPTURE && stream->count >= frame) {
        ready |= TABOS_WAIT_READABLE;
    }
    *returned_events = ready & requested_events;
    platform_mutex_unlock(audio_mutex);
    return 0;
}

void audio_service_render(int16_t* stereo, size_t frames)
{
    if (!initialized || stereo == NULL) {
        return;
    }
    memset(stereo, 0, frames * 2U * sizeof(*stereo));
    platform_mutex_lock(audio_mutex);
    for (size_t frame_index = 0U; frame_index < frames; ++frame_index) {
        int32_t left  = 0;
        int32_t right = 0;
        for (size_t stream_index = 0U; stream_index < AUDIO_STREAM_CAPACITY; ++stream_index) {
            audio_stream_t* stream = &streams[stream_index];
            if (!stream->open || stream->direction != TABOS_AUDIO_PLAYBACK) {
                continue;
            }
            if (stream->count < frame_bytes(stream)) {
                if (frame_index == 0U) {
                    ++stream->underruns;
                }
                continue;
            }
            const int32_t first =
                ((int32_t) ring_pop_sample(stream) * (int32_t) stream->volume) / TABOS_AUDIO_VOLUME_MAX;
            int32_t second = first;
            if (stream->channels == 2U) {
                second = ((int32_t) ring_pop_sample(stream) * (int32_t) stream->volume) / TABOS_AUDIO_VOLUME_MAX;
            }
            left  += first;
            right += second;
        }
        stereo[frame_index * 2U]      = saturate(left);
        stereo[frame_index * 2U + 1U] = saturate(right);
    }
    platform_mutex_unlock(audio_mutex);
}

void audio_service_capture(const int16_t* samples, size_t frames, uint32_t channels)
{
    if (!initialized || samples == NULL || channels == 0U) {
        return;
    }
    platform_mutex_lock(audio_mutex);
    for (size_t stream_index = 0U; stream_index < AUDIO_STREAM_CAPACITY; ++stream_index) {
        audio_stream_t* stream = &streams[stream_index];
        if (!stream->open || stream->direction != TABOS_AUDIO_CAPTURE) {
            continue;
        }
        const size_t frame = frame_bytes(stream);
        for (size_t frame_index = 0U; frame_index < frames; ++frame_index) {
            if (AUDIO_RING_CAPACITY - stream->count < frame) {
                for (size_t byte = 0U; byte < frame; ++byte) {
                    (void) ring_pop_byte(stream);
                }
                ++stream->overruns;
            }
            if (stream->channels == 1U) {
                int32_t sum = 0;
                for (uint32_t channel = 0U; channel < channels; ++channel) {
                    sum += samples[frame_index * channels + channel];
                }
                ring_push_sample(stream, (int16_t) (sum / (int32_t) channels));
            } else {
                for (uint32_t channel = 0U; channel < stream->channels; ++channel) {
                    const uint32_t source_channel = channel < channels ? channel : channels - 1U;
                    ring_push_sample(stream, samples[frame_index * channels + source_channel]);
                }
            }
        }
    }
    platform_mutex_unlock(audio_mutex);
}

void audio_service_error(int error)
{
    if (!initialized) {
        return;
    }
    platform_mutex_lock(audio_mutex);
    platform_info.ready = false;
    platform_info.error = error != 0 ? error : TABOS_EIO;
    for (size_t index = 0U; index < AUDIO_STREAM_CAPACITY; ++index) {
        if (streams[index].open) {
            streams[index].faulted = true;
        }
    }
    platform_mutex_unlock(audio_mutex);
}
