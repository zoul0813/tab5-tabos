#include <tabos/internal/camera.h>

#include <tabos/filesystem.h>
#include <tabos/platform/platform.h>
#include <tabos/wait.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    CAMERA_STREAM_CAPACITY       = 4,
    CAMERA_STREAM_INDEX_BITS     = 2,
    CAMERA_STREAM_INDEX_MASK     = CAMERA_STREAM_CAPACITY - 1,
    CAMERA_STREAM_GENERATION_MAX = INT32_MAX >> CAMERA_STREAM_INDEX_BITS,
    CAMERA_FRAME_CAPACITY        = 3,
    CAMERA_LEASE_INDEX_BITS      = 2,
    CAMERA_LEASE_INDEX_MASK      = CAMERA_FRAME_CAPACITY,
};

typedef struct {
        uint8_t* data;
        size_t capacity;
        size_t size;
        uint32_t generation;
        tabos_camera_frame_t metadata;
        bool ready;
        bool leased;
} camera_frame_slot_t;

typedef struct {
        const void* owner;
        camera_frame_slot_t frames[CAMERA_FRAME_CAPACITY];
        tabos_camera_config_t config;
        uint32_t generation;
        uint32_t sequence;
        uint32_t drops;
        bool open;
        bool faulted;
        bool hangup;
} camera_stream_t;

static camera_stream_t streams[CAMERA_STREAM_CAPACITY];
static platform_camera_info_t platform_info;
static platform_mutex_t* camera_mutex;
static tabos_device_id_t camera_device_id = TABOS_DEVICE_ID_INVALID;
static size_t open_count;
static bool initialized;

static uint32_t next_generation(uint32_t generation, uint32_t maximum)
{
    ++generation;
    return generation == 0U || generation > maximum ? 1U : generation;
}

static tabos_camera_stream_t stream_handle(size_t index, uint32_t generation)
{
    return (tabos_camera_stream_t) ((generation << CAMERA_STREAM_INDEX_BITS) | index);
}

static camera_stream_t* owned_stream(const void* owner, tabos_camera_stream_t handle)
{
    if (owner == NULL || handle <= 0) {
        return NULL;
    }
    const uint32_t value    = (uint32_t) handle;
    camera_stream_t* stream = &streams[value & CAMERA_STREAM_INDEX_MASK];
    return stream->open && stream->owner == owner && stream->generation == (value >> CAMERA_STREAM_INDEX_BITS) ?
               stream :
               NULL;
}

static tabos_camera_lease_t lease_handle(size_t index, uint32_t generation)
{
    return (generation << CAMERA_LEASE_INDEX_BITS) | (uint32_t) index;
}

static camera_frame_slot_t* leased_frame(camera_stream_t* stream, tabos_camera_lease_t lease)
{
    if (lease == TABOS_CAMERA_LEASE_INVALID) {
        return NULL;
    }
    const uint32_t index = lease & ((1U << CAMERA_LEASE_INDEX_BITS) - 1U);
    if (index >= CAMERA_FRAME_CAPACITY) {
        return NULL;
    }
    camera_frame_slot_t* frame = &stream->frames[index];
    return frame->leased && frame->generation == (lease >> CAMERA_LEASE_INDEX_BITS) ? frame : NULL;
}

bool camera_service_init(void)
{
    if (initialized) {
        return true;
    }
    memset(streams, 0, sizeof(streams));
    camera_mutex = platform_mutex_create();
    if (camera_mutex == NULL) {
        return false;
    }
    initialized = true;
    (void) platform_camera_init(camera_service_submit, camera_service_error, &platform_info);
    return true;
}

void camera_service_shutdown(void)
{
    if (!initialized) {
        return;
    }
    platform_camera_shutdown();
    camera_service_close_owner(NULL);
    platform_mutex_destroy(camera_mutex);
    camera_mutex     = NULL;
    platform_info    = (platform_camera_info_t) {0};
    camera_device_id = TABOS_DEVICE_ID_INVALID;
    open_count       = 0U;
    initialized      = false;
}

bool camera_service_info(tabos_camera_info_t* info, const char** driver, bool* ready, int* error)
{
    if (!initialized || !platform_info.detected) {
        return false;
    }
    platform_mutex_lock(camera_mutex);
    if (info != NULL) {
        *info = (tabos_camera_info_t) {.device_id  = camera_device_id,
                                       .formats    = platform_info.formats,
                                       .max_width  = platform_info.max_width,
                                       .max_height = platform_info.max_height,
                                       .max_fps    = platform_info.max_fps};
    }
    if (driver != NULL) {
        *driver = platform_info.driver;
    }
    if (ready != NULL) {
        *ready = platform_info.ready;
    }
    if (error != NULL) {
        *error = platform_info.error;
    }
    platform_mutex_unlock(camera_mutex);
    return true;
}

void camera_service_set_device_id(tabos_device_id_t device_id)
{
    camera_device_id = device_id;
}

static void free_stream(camera_stream_t* stream)
{
    for (size_t index = 0U; index < CAMERA_FRAME_CAPACITY; ++index) {
        free(stream->frames[index].data);
        stream->frames[index].data = NULL;
    }
    const uint32_t generation = stream->generation;
    *stream                   = (camera_stream_t) {.generation = generation};
}

tabos_camera_stream_t camera_service_open(const void* owner, const tabos_camera_config_t* config)
{
    if (!initialized || !platform_info.detected || !platform_info.ready || platform_info.error != 0) {
        return -TABOS_ENODEV;
    }
    if (owner == NULL || config == NULL || config->device_id != camera_device_id || config->width == 0U ||
        config->height == 0U || config->width > platform_info.max_width || config->height > platform_info.max_height ||
        config->fps == 0U || config->fps > platform_info.max_fps || config->format >= TABOS_CAMERA_FORMAT_COUNT ||
        (platform_info.formats & (1U << config->format)) == 0U) {
        return -TABOS_EINVAL;
    }
    if (config->width > SIZE_MAX / config->height) {
        return -TABOS_EINVAL;
    }
    const size_t pixels = (size_t) config->width * config->height;
    if (pixels > SIZE_MAX / 2U) {
        return -TABOS_EINVAL;
    }
    const size_t capacity = pixels * 2U;
    platform_mutex_lock(camera_mutex);
    if (open_count > 0U) {
        platform_mutex_unlock(camera_mutex);
        return -TABOS_EBUSY;
    }
    for (size_t index = 0U; index < CAMERA_STREAM_CAPACITY; ++index) {
        camera_stream_t* stream = &streams[index];
        if (stream->open) {
            continue;
        }
        uint8_t* allocations[CAMERA_FRAME_CAPACITY] = {0};
        bool allocated                              = true;
        for (size_t frame = 0U; frame < CAMERA_FRAME_CAPACITY; ++frame) {
            allocations[frame]  = malloc(capacity);
            allocated          &= allocations[frame] != NULL;
        }
        if (!allocated) {
            for (size_t frame = 0U; frame < CAMERA_FRAME_CAPACITY; ++frame) {
                free(allocations[frame]);
            }
            platform_mutex_unlock(camera_mutex);
            return -TABOS_ENOMEM;
        }
        const uint32_t generation = next_generation(stream->generation, CAMERA_STREAM_GENERATION_MAX);
        *stream = (camera_stream_t) {.owner = owner, .config = *config, .generation = generation, .open = true};
        for (size_t frame = 0U; frame < CAMERA_FRAME_CAPACITY; ++frame) {
            stream->frames[frame].data     = allocations[frame];
            stream->frames[frame].capacity = capacity;
        }
        if (!platform_camera_start(config)) {
            free_stream(stream);
            platform_mutex_unlock(camera_mutex);
            return -TABOS_EIO;
        }
        open_count                         = 1U;
        const tabos_camera_stream_t result = stream_handle(index, generation);
        platform_mutex_unlock(camera_mutex);
        return result;
    }
    platform_mutex_unlock(camera_mutex);
    return -TABOS_EMFILE;
}

int camera_service_close(const void* owner, tabos_camera_stream_t handle)
{
    platform_mutex_lock(camera_mutex);
    camera_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(camera_mutex);
        return -TABOS_EBADF;
    }
    platform_camera_stop();
    free_stream(stream);
    open_count = 0U;
    platform_mutex_unlock(camera_mutex);
    return 0;
}

int camera_service_acquire(const void* owner, tabos_camera_stream_t handle, tabos_camera_frame_t* frame)
{
    if (frame == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(camera_mutex);
    camera_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(camera_mutex);
        return -TABOS_EBADF;
    }
    camera_frame_slot_t* slot = NULL;
    size_t slot_index         = 0U;
    uint32_t oldest_sequence  = UINT32_MAX;
    for (size_t index = 0U; index < CAMERA_FRAME_CAPACITY; ++index) {
        camera_frame_slot_t* candidate = &stream->frames[index];
        if (candidate->ready && !candidate->leased && candidate->metadata.sequence < oldest_sequence) {
            slot            = candidate;
            slot_index      = index;
            oldest_sequence = candidate->metadata.sequence;
        }
    }
    if (slot != NULL) {
        slot->ready                   = false;
        slot->leased                  = true;
        slot->generation              = next_generation(slot->generation, UINT32_MAX >> CAMERA_LEASE_INDEX_BITS);
        slot->metadata.lease          = lease_handle(slot_index, slot->generation);
        slot->metadata.dropped_frames = stream->drops;
        *frame                        = slot->metadata;
        platform_mutex_unlock(camera_mutex);
        return 0;
    }
    const int result = stream->faulted ? -TABOS_EIO : (stream->hangup ? -TABOS_ENODEV : -TABOS_EAGAIN);
    platform_mutex_unlock(camera_mutex);
    return result;
}

int camera_service_copy(const void* owner, tabos_camera_stream_t handle, tabos_camera_lease_t lease, uint32_t offset,
                        void* buffer, uint32_t capacity)
{
    if (buffer == NULL || capacity == 0U) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(camera_mutex);
    camera_stream_t* stream    = owned_stream(owner, handle);
    camera_frame_slot_t* frame = stream != NULL ? leased_frame(stream, lease) : NULL;
    if (frame == NULL) {
        platform_mutex_unlock(camera_mutex);
        return -TABOS_EBADF;
    }
    if (offset >= frame->size) {
        platform_mutex_unlock(camera_mutex);
        return 0;
    }
    size_t bytes = frame->size - offset;
    if (bytes > capacity) {
        bytes = capacity;
    }
    memcpy(buffer, frame->data + offset, bytes);
    platform_mutex_unlock(camera_mutex);
    return (int) bytes;
}

int camera_service_release(const void* owner, tabos_camera_stream_t handle, tabos_camera_lease_t lease)
{
    platform_mutex_lock(camera_mutex);
    camera_stream_t* stream    = owned_stream(owner, handle);
    camera_frame_slot_t* frame = stream != NULL ? leased_frame(stream, lease) : NULL;
    if (frame == NULL) {
        platform_mutex_unlock(camera_mutex);
        return -TABOS_EBADF;
    }
    frame->leased = false;
    platform_mutex_unlock(camera_mutex);
    return 0;
}

int camera_service_poll(const void* owner, tabos_camera_stream_t handle, uint32_t requested_events,
                        uint32_t* returned_events)
{
    if (returned_events == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(camera_mutex);
    camera_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(camera_mutex);
        return -TABOS_EBADF;
    }
    uint32_t result = 0U;
    for (size_t index = 0U; index < CAMERA_FRAME_CAPACITY; ++index) {
        if (stream->frames[index].ready && !stream->frames[index].leased) {
            result |= requested_events & TABOS_WAIT_READABLE;
            break;
        }
    }
    if (stream->faulted) {
        result |= requested_events & TABOS_WAIT_ERROR;
    }
    if (stream->hangup) {
        result |= requested_events & TABOS_WAIT_HANGUP;
    }
    *returned_events = result;
    platform_mutex_unlock(camera_mutex);
    return 0;
}

void camera_service_close_owner(const void* owner)
{
    if (!initialized) {
        return;
    }
    platform_mutex_lock(camera_mutex);
    for (size_t index = 0U; index < CAMERA_STREAM_CAPACITY; ++index) {
        if (streams[index].open && (owner == NULL || streams[index].owner == owner)) {
            platform_camera_stop();
            free_stream(&streams[index]);
            open_count = 0U;
        }
    }
    platform_mutex_unlock(camera_mutex);
}

void camera_service_submit(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t stride_bytes,
                           uint32_t format, uint64_t timestamp_ms)
{
    if (!initialized || data == NULL || size == 0U || format >= TABOS_CAMERA_FORMAT_COUNT) {
        return;
    }
    platform_mutex_lock(camera_mutex);
    for (size_t stream_index = 0U; stream_index < CAMERA_STREAM_CAPACITY; ++stream_index) {
        camera_stream_t* stream = &streams[stream_index];
        if (!stream->open || stream->faulted || stream->hangup) {
            continue;
        }
        const bool packed = format == TABOS_CAMERA_FORMAT_RAW8 || format == TABOS_CAMERA_FORMAT_RGB565;
        if (format != (uint32_t) stream->config.format || width != stream->config.width ||
            height != stream->config.height || (packed && stride_bytes == 0U)) {
            continue;
        }
        camera_frame_slot_t* selected = NULL;
        for (size_t index = 0U; index < CAMERA_FRAME_CAPACITY; ++index) {
            if (!stream->frames[index].leased && !stream->frames[index].ready) {
                selected = &stream->frames[index];
                break;
            }
        }
        if (selected == NULL) {
            uint32_t oldest = UINT32_MAX;
            for (size_t index = 0U; index < CAMERA_FRAME_CAPACITY; ++index) {
                if (!stream->frames[index].leased && stream->frames[index].metadata.sequence < oldest) {
                    oldest   = stream->frames[index].metadata.sequence;
                    selected = &stream->frames[index];
                }
            }
            if (selected == NULL) {
                ++stream->drops;
                continue;
            }
            ++stream->drops;
        }
        const size_t bytes = size < selected->capacity ? size : selected->capacity;
        memcpy(selected->data, data, bytes);
        selected->size     = bytes;
        selected->ready    = true;
        selected->metadata = (tabos_camera_frame_t) {.format       = (tabos_camera_format_t) format,
                                                     .width        = width,
                                                     .height       = height,
                                                     .stride_bytes = stride_bytes,
                                                     .size_bytes   = (uint32_t) bytes,
                                                     .timestamp_ms = timestamp_ms,
                                                     .sequence     = ++stream->sequence};
    }
    platform_mutex_unlock(camera_mutex);
}

void camera_service_error(int error)
{
    platform_mutex_lock(camera_mutex);
    platform_info.error = error;
    for (size_t index = 0U; index < CAMERA_STREAM_CAPACITY; ++index) {
        streams[index].faulted |= streams[index].open;
    }
    platform_mutex_unlock(camera_mutex);
}

void camera_service_remove_device(void)
{
    if (!initialized) {
        return;
    }
    platform_mutex_lock(camera_mutex);
    platform_camera_stop();
    for (size_t index = 0U; index < CAMERA_STREAM_CAPACITY; ++index) {
        streams[index].hangup |= streams[index].open;
    }
    platform_info.detected = false;
    platform_info.ready    = false;
    platform_mutex_unlock(camera_mutex);
}
