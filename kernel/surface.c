#include <tabos/internal/surface.h>
#include <tabos/internal/ipc.h>
#include <tabos/config/gui.h>
#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    SURFACE_CAPACITY   = 16,
    SURFACE_MAX_WIDTH  = 1280,
    SURFACE_MAX_HEIGHT = 720
};
typedef struct {
        int handle;
        uint32_t owner;
        uint32_t reader;
        uint32_t width, height, revision;
        size_t bytes;
        uint16_t* committed;
        uint16_t* staging;
} surface_entry_t;

static surface_entry_t surfaces[SURFACE_CAPACITY];
static platform_mutex_t* mutex;
static uint32_t generation = 1U;
static size_t used_bytes;
static size_t peak_bytes;

bool surface_service_init(void)
{
    if (mutex == NULL) {
        mutex = platform_mutex_create();
    }
    return mutex != NULL;
}

static surface_entry_t* find_surface(int handle)
{
    if (handle <= 0) {
        return NULL;
    }
    surface_entry_t* entry = &surfaces[(unsigned int) handle % SURFACE_CAPACITY];
    return entry->handle == handle ? entry : NULL;
}

static void abort_upload(surface_entry_t* entry)
{
    if (entry->staging != NULL) {
        free(entry->staging);
        entry->staging  = NULL;
        used_bytes     -= entry->bytes;
    }
}

static void release_surface(surface_entry_t* entry)
{
    abort_upload(entry);
    free(entry->committed);
    used_bytes -= entry->bytes;
    *entry      = (surface_entry_t) {0};
}

void surface_service_close_owner(uint32_t owner)
{
    if (mutex == NULL) {
        return;
    }
    platform_mutex_lock(mutex);
    for (unsigned int index = 0U; index < SURFACE_CAPACITY; ++index) {
        surface_entry_t* entry = &surfaces[index];
        if (entry->handle != 0 && entry->owner == owner) {
            release_surface(entry);
        } else if (entry->reader == owner) {
            entry->reader = 0U;
        }
    }
    platform_mutex_unlock(mutex);
}

void surface_service_shutdown(void)
{
    if (mutex == NULL) {
        return;
    }
    for (unsigned int index = 0U; index < SURFACE_CAPACITY; ++index) {
        if (surfaces[index].handle != 0) {
            release_surface(&surfaces[index]);
        }
    }
    platform_mutex_destroy(mutex);
    mutex      = NULL;
    peak_bytes = 0U;
}

static bool reserve_bytes(uint32_t owner, size_t bytes)
{
    size_t owner_bytes = 0U;
    for (unsigned int index = 0U; index < SURFACE_CAPACITY; ++index) {
        const surface_entry_t* entry = &surfaces[index];
        if (entry->handle != 0 && entry->owner == owner) {
            owner_bytes += entry->bytes;
            if (entry->staging != NULL) {
                owner_bytes += entry->bytes;
            }
        }
    }
    return bytes <= TABOS_GUI_SURFACE_BYTES && used_bytes <= TABOS_GUI_SURFACE_BYTES - bytes &&
           bytes <= TABOS_GUI_PROCESS_SURFACE_BYTES && owner_bytes <= TABOS_GUI_PROCESS_SURFACE_BYTES - bytes;
}

static void account_bytes(size_t bytes)
{
    used_bytes += bytes;
    if (used_bytes > peak_bytes) {
        peak_bytes = used_bytes;
    }
}

static bool valid_size(uint32_t width, uint32_t height)
{
    return width != 0U && height != 0U && width <= SURFACE_MAX_WIDTH && height <= SURFACE_MAX_HEIGHT;
}

static int request_locked(uint32_t owner, uint32_t operation, surface_transport_packet_t* packet, void* pixels,
                          size_t pixel_bytes, uint32_t reader)
{
    if (operation == SURFACE_TRANSPORT_STATS) {
        packet->stats = (tabos_surface_stats_t) {(uint32_t) used_bytes, (uint32_t) peak_bytes, TABOS_GUI_SURFACE_BYTES,
                                                 TABOS_GUI_PROCESS_SURFACE_BYTES};
        return 0;
    }
    if (operation == SURFACE_TRANSPORT_CREATE) {
        if (!valid_size(packet->width, packet->height)) {
            return -TABOS_EINVAL;
        }
        const size_t bytes = (size_t) packet->width * packet->height * sizeof(uint16_t);
        if (!reserve_bytes(owner, bytes) || generation >= (uint32_t) INT_MAX / SURFACE_CAPACITY) {
            return -TABOS_ENOMEM;
        }
        for (unsigned int index = 0U; index < SURFACE_CAPACITY; ++index) {
            if (surfaces[index].handle == 0) {
                uint16_t* committed = calloc(1U, bytes);
                if (committed == NULL) {
                    return -TABOS_ENOMEM;
                }
                surfaces[index] = (surface_entry_t) {.handle    = (int) (generation++ * SURFACE_CAPACITY + index),
                                                     .owner     = owner,
                                                     .width     = packet->width,
                                                     .height    = packet->height,
                                                     .bytes     = bytes,
                                                     .committed = committed};
                account_bytes(bytes);
                return surfaces[index].handle;
            }
        }
        return -TABOS_ENOMEM;
    }
    surface_entry_t* entry    = find_surface(packet->surface);
    const bool read_operation = operation == SURFACE_TRANSPORT_READ || operation == SURFACE_TRANSPORT_INFO;
    if (entry == NULL || (entry->owner != owner && (!read_operation || entry->reader != owner))) {
        return -TABOS_EBADF;
    }
    switch (operation) {
        case SURFACE_TRANSPORT_RELEASE: release_surface(entry); return 0;
        case SURFACE_TRANSPORT_GRANT: entry->reader = reader; return 0;
        case SURFACE_TRANSPORT_INFO:
            packet->info = (tabos_surface_info_t) {entry->width, entry->height, entry->revision};
            return 0;
        case SURFACE_TRANSPORT_ABORT: abort_upload(entry); return 0;
        case SURFACE_TRANSPORT_COMMIT:
            if (entry->staging != NULL) {
                uint16_t* previous = entry->committed;
                entry->committed   = entry->staging;
                entry->staging     = NULL;
                free(previous);
                used_bytes -= entry->bytes;
                ++entry->revision;
            }
            return 0;
        case SURFACE_TRANSPORT_READ:
        case SURFACE_TRANSPORT_UPLOAD: break;
        default: return -TABOS_EINVAL;
    }
    if (!valid_size(packet->width, packet->height) || packet->x > entry->width || packet->y > entry->height ||
        packet->width > entry->width - packet->x || packet->height > entry->height - packet->y || pixels == NULL ||
        pixel_bytes < (size_t) packet->width * packet->height * sizeof(uint16_t)) {
        if (operation == SURFACE_TRANSPORT_UPLOAD) {
            abort_upload(entry);
        }
        return -TABOS_EINVAL;
    }
    if (operation == SURFACE_TRANSPORT_UPLOAD && entry->staging == NULL) {
        if (!reserve_bytes(owner, entry->bytes)) {
            return -TABOS_ENOMEM;
        }
        entry->staging = malloc(entry->bytes);
        if (entry->staging == NULL) {
            return -TABOS_ENOMEM;
        }
        memcpy(entry->staging, entry->committed, entry->bytes);
        account_bytes(entry->bytes);
    }
    for (uint32_t row = 0U; row < packet->height; ++row) {
        const size_t offset   = (size_t) (packet->y + row) * entry->width + packet->x;
        uint16_t* transferred = (uint16_t*) pixels + (size_t) row * packet->width;
        if (operation == SURFACE_TRANSPORT_READ) {
            memcpy(transferred, entry->committed + offset, (size_t) packet->width * sizeof(uint16_t));
        } else {
            memcpy(entry->staging + offset, transferred, (size_t) packet->width * sizeof(uint16_t));
        }
    }
    return 0;
}

int surface_service_request(uint32_t owner, uint32_t operation, surface_transport_packet_t* packet, void* pixels,
                            size_t pixel_bytes)
{
    if (mutex == NULL || packet == NULL || owner == 0U) {
        return -TABOS_EINVAL;
    }
    uint32_t reader = 0U;
    if (operation == SURFACE_TRANSPORT_GRANT && ipc_service_peer(owner, packet->channel, &reader) != 0) {
        return -TABOS_EBADF;
    }
    platform_mutex_lock(mutex);
    const int result = request_locked(owner, operation, packet, pixels, pixel_bytes, reader);
    platform_mutex_unlock(mutex);
    return result;
}
