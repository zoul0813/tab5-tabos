#include <tabos/internal/pointer.h>

#include <tabos/config/display.h>
#include <tabos/filesystem.h>
#include <tabos/platform/platform.h>
#include <tabos/wait.h>

#include <limits.h>
#include <string.h>

enum {
    POINTER_STREAM_CAPACITY       = 8,
    POINTER_STREAM_INDEX_BITS     = 3,
    POINTER_STREAM_INDEX_MASK     = POINTER_STREAM_CAPACITY - 1,
    POINTER_STREAM_GENERATION_MAX = INT32_MAX >> POINTER_STREAM_INDEX_BITS,
    POINTER_QUEUE_CAPACITY        = 64,
};

_Static_assert((1U << POINTER_STREAM_INDEX_BITS) == POINTER_STREAM_CAPACITY,
               "pointer stream index bits must match capacity");

typedef struct {
        const void* owner;
        tabos_pointer_event_t queue[POINTER_QUEUE_CAPACITY];
        bool contacts[TABOS_POINTER_MAX_CONTACTS];
        int32_t contact_x[TABOS_POINTER_MAX_CONTACTS];
        int32_t contact_y[TABOS_POINTER_MAX_CONTACTS];
        uint32_t generation;
        size_t head;
        size_t count;
        bool open;
        bool hangup;
} pointer_stream_t;

static pointer_stream_t streams[POINTER_STREAM_CAPACITY];
static platform_mutex_t* pointer_mutex;
static const void* foreground_owner;
static const char* pointer_driver;
static tabos_device_id_t pointer_device_id = TABOS_DEVICE_ID_INVALID;
static int pointer_error;
static bool detected;
static bool initialized;

static uint32_t next_generation(uint32_t generation)
{
    ++generation;
    if (generation == 0U || generation > POINTER_STREAM_GENERATION_MAX) {
        generation = 1U;
    }
    return generation;
}

static tabos_pointer_stream_t stream_handle(size_t index, uint32_t generation)
{
    return (tabos_pointer_stream_t) ((generation << POINTER_STREAM_INDEX_BITS) | (uint32_t) index);
}

static pointer_stream_t* owned_stream(const void* owner, tabos_pointer_stream_t handle)
{
    if (owner == NULL || handle <= 0) {
        return NULL;
    }
    const uint32_t value      = (uint32_t) handle;
    const uint32_t index      = value & POINTER_STREAM_INDEX_MASK;
    const uint32_t generation = value >> POINTER_STREAM_INDEX_BITS;
    pointer_stream_t* stream  = &streams[index];
    if (!stream->open || stream->owner != owner || stream->generation != generation) {
        return NULL;
    }
    return stream;
}

static void enqueue(pointer_stream_t* stream, const tabos_pointer_event_t* event)
{
    const size_t tail   = (stream->head + stream->count) % POINTER_QUEUE_CAPACITY;
    stream->queue[tail] = *event;
    ++stream->count;
}

static void cancel_contacts(pointer_stream_t* stream)
{
    stream->head  = 0U;
    stream->count = 0U;
    for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
        if (!stream->contacts[contact]) {
            continue;
        }
        const tabos_pointer_event_t cancel = {
            .type       = TABOS_POINTER_CANCEL,
            .device_id  = pointer_device_id,
            .contact_id = contact,
            .x          = stream->contact_x[contact],
            .y          = stream->contact_y[contact],
        };
        enqueue(stream, &cancel);
        stream->contacts[contact] = false;
    }
}

bool pointer_service_init(void)
{
    if (initialized) {
        return true;
    }
    memset(streams, 0, sizeof(streams));
    pointer_mutex = platform_mutex_create();
    if (pointer_mutex == NULL) {
        return false;
    }
    initialized = true;
    detected    = platform_pointer_init(&pointer_driver, &pointer_error);
    return true;
}

void pointer_service_shutdown(void)
{
    if (!initialized) {
        return;
    }
    platform_pointer_shutdown();
    platform_mutex_lock(pointer_mutex);
    memset(streams, 0, sizeof(streams));
    foreground_owner  = NULL;
    pointer_driver    = NULL;
    pointer_device_id = TABOS_DEVICE_ID_INVALID;
    pointer_error     = 0;
    detected          = false;
    platform_mutex_unlock(pointer_mutex);
    platform_mutex_destroy(pointer_mutex);
    pointer_mutex = NULL;
    initialized   = false;
}

bool pointer_service_info(const char** driver, int* error)
{
    if (!initialized || !detected) {
        return false;
    }
    platform_mutex_lock(pointer_mutex);
    if (driver != NULL) {
        *driver = pointer_driver;
    }
    if (error != NULL) {
        *error = pointer_error;
    }
    platform_mutex_unlock(pointer_mutex);
    return true;
}

void pointer_service_set_device_id(tabos_device_id_t device_id)
{
    if (!initialized) {
        return;
    }
    platform_mutex_lock(pointer_mutex);
    pointer_device_id = device_id;
    platform_mutex_unlock(pointer_mutex);
}

void pointer_service_set_foreground_owner(const void* owner)
{
    if (!initialized) {
        return;
    }
    platform_mutex_lock(pointer_mutex);
    if (foreground_owner != owner) {
        for (size_t index = 0U; index < POINTER_STREAM_CAPACITY; ++index) {
            if (streams[index].open && streams[index].owner == foreground_owner) {
                cancel_contacts(&streams[index]);
            }
        }
        foreground_owner = owner;
    }
    platform_mutex_unlock(pointer_mutex);
}

tabos_pointer_stream_t pointer_service_open(const void* owner, tabos_device_id_t device_id)
{
    if (!initialized || !detected || pointer_error != 0) {
        return -TABOS_ENODEV;
    }
    if (owner == NULL || device_id == TABOS_DEVICE_ID_INVALID || device_id != pointer_device_id) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(pointer_mutex);
    for (size_t index = 0U; index < POINTER_STREAM_CAPACITY; ++index) {
        if (streams[index].open) {
            continue;
        }
        const uint32_t generation = next_generation(streams[index].generation);
        streams[index]            = (pointer_stream_t) {
                       .owner      = owner,
                       .generation = generation,
                       .open       = true,
        };
        const tabos_pointer_stream_t result = stream_handle(index, generation);
        platform_mutex_unlock(pointer_mutex);
        return result;
    }
    platform_mutex_unlock(pointer_mutex);
    return -TABOS_EMFILE;
}

int pointer_service_close(const void* owner, tabos_pointer_stream_t handle)
{
    if (!initialized) {
        return -TABOS_EBADF;
    }
    platform_mutex_lock(pointer_mutex);
    pointer_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(pointer_mutex);
        return -TABOS_EBADF;
    }
    const uint32_t generation = stream->generation;
    *stream                   = (pointer_stream_t) {.generation = generation};
    platform_mutex_unlock(pointer_mutex);
    return 0;
}

int pointer_service_read(const void* owner, tabos_pointer_stream_t handle, tabos_pointer_event_t* event)
{
    if (!initialized || event == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(pointer_mutex);
    pointer_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(pointer_mutex);
        return -TABOS_EBADF;
    }
    if (owner != foreground_owner) {
        platform_mutex_unlock(pointer_mutex);
        return -TABOS_EACCES;
    }
    if (stream->count == 0U) {
        const int result = stream->hangup ? -TABOS_ENODEV : -TABOS_EAGAIN;
        platform_mutex_unlock(pointer_mutex);
        return result;
    }
    *event       = stream->queue[stream->head];
    stream->head = (stream->head + 1U) % POINTER_QUEUE_CAPACITY;
    --stream->count;
    platform_mutex_unlock(pointer_mutex);
    return 0;
}

int pointer_service_poll(const void* owner, tabos_pointer_stream_t handle, uint32_t requested_events,
                         uint32_t* returned_events)
{
    if (!initialized || returned_events == NULL) {
        return -TABOS_EINVAL;
    }
    platform_mutex_lock(pointer_mutex);
    pointer_stream_t* stream = owned_stream(owner, handle);
    if (stream == NULL) {
        platform_mutex_unlock(pointer_mutex);
        return -TABOS_EBADF;
    }
    uint32_t result = 0U;
    if (owner == foreground_owner && stream->count > 0U && (requested_events & TABOS_WAIT_READABLE) != 0U) {
        result |= TABOS_WAIT_READABLE;
    }
    if (stream->hangup && (requested_events & TABOS_WAIT_HANGUP) != 0U) {
        result |= TABOS_WAIT_HANGUP;
    }
    *returned_events = result;
    platform_mutex_unlock(pointer_mutex);
    return 0;
}

void pointer_service_close_owner(const void* owner)
{
    if (!initialized || owner == NULL) {
        return;
    }
    platform_mutex_lock(pointer_mutex);
    for (size_t index = 0U; index < POINTER_STREAM_CAPACITY; ++index) {
        if (streams[index].open && streams[index].owner == owner) {
            const uint32_t generation = streams[index].generation;
            streams[index]            = (pointer_stream_t) {.generation = generation};
        }
    }
    if (foreground_owner == owner) {
        foreground_owner = NULL;
    }
    platform_mutex_unlock(pointer_mutex);
}

void pointer_service_submit(const tabos_pointer_event_t* event)
{
    if (!initialized || event == NULL || event->type >= TABOS_POINTER_EVENT_TYPE_COUNT ||
        event->contact_id >= TABOS_POINTER_MAX_CONTACTS || event->x < 0 || event->y < 0 ||
        event->x >= TABOS_DISPLAY_WIDTH || event->y >= TABOS_DISPLAY_HEIGHT) {
        return;
    }
    platform_mutex_lock(pointer_mutex);
    for (size_t index = 0U; index < POINTER_STREAM_CAPACITY; ++index) {
        pointer_stream_t* stream = &streams[index];
        if (!stream->open || stream->owner != foreground_owner || stream->hangup) {
            continue;
        }
        if (stream->count == POINTER_QUEUE_CAPACITY) {
            cancel_contacts(stream);
        }
        tabos_pointer_event_t normalized = *event;
        normalized.device_id             = pointer_device_id;
        enqueue(stream, &normalized);
        if (event->type == TABOS_POINTER_DOWN || event->type == TABOS_POINTER_MOVE) {
            stream->contacts[event->contact_id]  = true;
            stream->contact_x[event->contact_id] = event->x;
            stream->contact_y[event->contact_id] = event->y;
        } else {
            stream->contacts[event->contact_id] = false;
        }
    }
    platform_mutex_unlock(pointer_mutex);
}

void pointer_service_remove_device(void)
{
    if (!initialized) {
        return;
    }
    platform_mutex_lock(pointer_mutex);
    for (size_t index = 0U; index < POINTER_STREAM_CAPACITY; ++index) {
        if (streams[index].open) {
            cancel_contacts(&streams[index]);
            streams[index].hangup = true;
        }
    }
    detected = false;
    platform_mutex_unlock(pointer_mutex);
}
