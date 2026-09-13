#include <tabos/internal/surface.h>
#include <tabos/internal/ipc.h>
#include <tabos/filesystem.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

static bool fail_allocation;
static unsigned int live_allocations;

static void* failing_calloc(size_t count, size_t size)
{
    if (fail_allocation) {
        return NULL;
    }
    void* result = calloc(count, size);
    if (result != NULL) {
        ++live_allocations;
    }
    return result;
}

static void* failing_malloc(size_t size)
{
    if (fail_allocation) {
        return NULL;
    }
    void* result = malloc(size);
    if (result != NULL) {
        ++live_allocations;
    }
    return result;
}

static void tracked_free(void* allocation)
{
    if (allocation != NULL) {
        assert(live_allocations > 0U);
        --live_allocations;
    }
    free(allocation);
}

/* Inject surface buffers only; IPC and platform allocations remain unchanged. */
#define calloc failing_calloc
#define malloc failing_malloc
#define free   tracked_free
#include "../../kernel/surface.c"
#undef free
#undef malloc
#undef calloc

static int concurrent_surface;
static atomic_bool writer_done;

static void* publish_frames(void* unused)
{
    (void) unused;
    uint16_t frame[64U * 64U];
    surface_transport_packet_t packet = {.surface = concurrent_surface, .width = 64U, .height = 64U};
    for (uint16_t value = 1U; value <= 500U; ++value) {
        for (unsigned int index = 0U; index < 64U * 64U; ++index) {
            frame[index] = value;
        }
        assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, frame, sizeof(frame)) == 0);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_COMMIT, &packet, NULL, 0U) == 0);
    }
    atomic_store_explicit(&writer_done, true, memory_order_release);
    return NULL;
}

static int operation(uint32_t owner, uint32_t op, int surface)
{
    surface_transport_packet_t packet = {.surface = surface};
    return surface_service_request(owner, op, &packet, NULL, 0U);
}

static void allocation_failures(void)
{
    assert(surface_service_init());
    for (unsigned int round = 0U; round < 100U; ++round) {
        surface_transport_packet_t packet = {.width = 2U, .height = 2U};
        fail_allocation                   = true;
        assert(surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &packet, NULL, 0U) == -TABOS_ENOMEM);
        assert(live_allocations == 0U);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_STATS, &packet, NULL, 0U) == 0);
        assert(packet.stats.used_bytes == 0U);
        fail_allocation  = false;
        const int handle = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &packet, NULL, 0U);
        assert(handle > 0 && live_allocations == 1U);
        packet.surface          = handle;
        uint16_t original[4]    = {1U, 2U, 3U, 4U};
        uint16_t replacement[4] = {5U, 6U, 7U, 8U};
        uint16_t copied[4]      = {0U};
        assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, original, sizeof(original)) == 0);
        assert(operation(2U, SURFACE_TRANSPORT_COMMIT, handle) == 0);
        fail_allocation = true;
        assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, replacement, sizeof(replacement)) ==
               -TABOS_ENOMEM);
        assert(operation(2U, SURFACE_TRANSPORT_COMMIT, handle) == 0);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &packet, copied, sizeof(copied)) == 0);
        assert(memcmp(copied, original, sizeof(original)) == 0);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_INFO, &packet, NULL, 0U) == 0);
        assert(packet.info.revision == 1U);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_STATS, &packet, NULL, 0U) == 0);
        assert(packet.stats.used_bytes == sizeof(original) && live_allocations == 1U);
        fail_allocation = false;
        assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, replacement, sizeof(replacement)) == 0);
        assert(operation(2U, SURFACE_TRANSPORT_COMMIT, handle) == 0);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &packet, copied, sizeof(copied)) == 0);
        assert(memcmp(copied, replacement, sizeof(replacement)) == 0);
        /* Teardown must reclaim both a retained frame and an unfinished upload. */
        assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, original, sizeof(original)) == 0);
        assert(live_allocations == 2U);
        surface_service_close_owner(2U);
        assert(live_allocations == 0U);
        assert(operation(2U, SURFACE_TRANSPORT_COMMIT, handle) == -TABOS_EBADF);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_STATS, &packet, NULL, 0U) == 0);
        assert(packet.stats.used_bytes == 0U);
    }
    surface_service_shutdown();
}

static void invalid_transfers(void)
{
    assert(surface_service_init());
    const uint32_t dimensions[][2] = {
        {        0U,         1U},
        {        1U,         0U},
        {     1281U,         1U},
        {        1U,       721U},
        {UINT32_MAX, UINT32_MAX}
    };
    for (size_t index = 0U; index < sizeof(dimensions) / sizeof(dimensions[0]); ++index) {
        surface_transport_packet_t invalid = {.width = dimensions[index][0], .height = dimensions[index][1]};
        assert(surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &invalid, NULL, 0U) == -TABOS_EINVAL);
        assert(live_allocations == 0U);
    }
    surface_transport_packet_t packet = {.width = 4U, .height = 4U};
    packet.surface                    = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &packet, NULL, 0U);
    assert(packet.surface > 0);
    uint16_t original[16], replacement[16];
    for (size_t index = 0U; index < 16U; ++index) {
        original[index]    = (uint16_t) index;
        replacement[index] = 0xabcdU;
    }
    assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, original, sizeof(original)) == 0);
    assert(operation(2U, SURFACE_TRANSPORT_COMMIT, packet.surface) == 0);
    const struct {
            uint32_t x, y, width, height;
            size_t bytes;
            bool null_pixels;
    } invalid[] = {
        {        0U,         0U,         0U,         1U, 32U, false},
        {        0U,         0U,         1U,         0U, 32U, false},
        {        4U,         0U,         1U,         1U, 32U, false},
        {        0U,         4U,         1U,         1U, 32U, false},
        {        3U,         0U,         2U,         1U, 32U, false},
        {        0U,         3U,         1U,         2U, 32U, false},
        {UINT32_MAX,         0U,         1U,         1U, 32U, false},
        {        0U, UINT32_MAX,         1U,         1U, 32U, false},
        {        0U,         0U, UINT32_MAX,         1U, 32U, false},
        {        0U,         0U,         1U, UINT32_MAX, 32U, false},
        {        0U,         0U,         4U,         4U, 31U, false},
        {        0U,         0U,         1U,         1U,  0U, false},
        {        0U,         0U,         4U,         4U, 32U,  true},
    };
    for (unsigned int round = 0U; round < 100U; ++round) {
        for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
            assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, replacement, sizeof(replacement)) ==
                   0);
            surface_transport_packet_t bad = {.surface = packet.surface,
                                              .x       = invalid[index].x,
                                              .y       = invalid[index].y,
                                              .width   = invalid[index].width,
                                              .height  = invalid[index].height};
            uint16_t guarded[18];
            memset(guarded, 0x5a, sizeof(guarded));
            void* buffer = invalid[index].null_pixels ? NULL : guarded + 1;
            assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &bad, buffer, invalid[index].bytes) ==
                   -TABOS_EINVAL);
            for (size_t pixel = 0U; pixel < 18U; ++pixel) {
                assert(guarded[pixel] == 0x5a5aU);
            }
            assert(live_allocations == 2U); /* Failed read cannot discard an owner's staging transaction. */
            assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &bad, buffer, invalid[index].bytes) ==
                   -TABOS_EINVAL);
            assert(live_allocations == 1U);
            assert(operation(2U, SURFACE_TRANSPORT_COMMIT, packet.surface) == 0);
            assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &packet, guarded + 1, sizeof(original)) == 0);
            assert(guarded[0] == 0x5a5aU && guarded[17] == 0x5a5aU);
            assert(memcmp(guarded + 1, original, sizeof(original)) == 0);
            assert(surface_service_request(2U, SURFACE_TRANSPORT_INFO, &packet, NULL, 0U) == 0);
            assert(packet.info.revision == 1U);
        }
    }
    surface_service_close_owner(2U);
    assert(surface_service_request(2U, SURFACE_TRANSPORT_STATS, &packet, NULL, 0U) == 0);
    assert(packet.stats.used_bytes == 0U && live_allocations == 0U);
    surface_service_shutdown();
}

typedef struct {
        pthread_mutex_t gate;
        pthread_cond_t changed;
        unsigned int ready;
        bool start;
        surface_transport_packet_t packet;
        int committed, read;
        uint16_t pixels[16];
} teardown_race_t;

static void race_start(teardown_race_t* race)
{
    assert(pthread_mutex_lock(&race->gate) == 0);
    ++race->ready;
    assert(pthread_cond_broadcast(&race->changed) == 0);
    while (!race->start) {
        assert(pthread_cond_wait(&race->changed, &race->gate) == 0);
    }
    assert(pthread_mutex_unlock(&race->gate) == 0);
}

static void* race_commit(void* argument)
{
    teardown_race_t* race = argument;
    race_start(race);
    surface_transport_packet_t packet = race->packet;
    race->committed                   = surface_service_request(2U, SURFACE_TRANSPORT_COMMIT, &packet, NULL, 0U);
    return NULL;
}

static void* race_read(void* argument)
{
    teardown_race_t* race = argument;
    race_start(race);
    surface_transport_packet_t packet = race->packet;
    race->read = surface_service_request(1U, SURFACE_TRANSPORT_READ, &packet, race->pixels, sizeof(race->pixels));
    return NULL;
}

static void teardown_races(void)
{
    assert(ipc_service_init() && surface_service_init());
    ipc_transport_packet_t connection = {0};
    assert(ipc_service_request(1U, 1U, IPC_TRANSPORT_LISTEN, &connection) > 0);
    const int channel = ipc_service_request(2U, 1U, IPC_TRANSPORT_CONNECT, &connection);
    assert(channel > 0);
    for (unsigned int round = 0U; round < 102U; ++round) {
        teardown_race_t race = {
            .gate    = PTHREAD_MUTEX_INITIALIZER,
            .changed = PTHREAD_COND_INITIALIZER,
            .packet  = {.width = 4U, .height = 4U, .channel = channel}
        };
        race.packet.surface = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &race.packet, NULL, 0U);
        assert(race.packet.surface > 0);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_GRANT, &race.packet, NULL, 0U) == 0);
        uint16_t frame[16];
        for (size_t index = 0U; index < 16U; ++index) {
            frame[index] = 0x1234U;
        }
        assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &race.packet, frame, sizeof(frame)) == 0);
        pthread_t writer, reader;
        assert(pthread_create(&writer, NULL, race_commit, &race) == 0);
        assert(pthread_create(&reader, NULL, race_read, &race) == 0);
        assert(pthread_mutex_lock(&race.gate) == 0);
        while (race.ready < 2U) {
            assert(pthread_cond_wait(&race.changed, &race.gate) == 0);
        }
        if (round % 3U == 0U) {
            surface_service_close_owner(2U); /* Deterministic teardown-before-service ordering. */
        }
        race.start = true;
        assert(pthread_cond_broadcast(&race.changed) == 0);
        assert(pthread_mutex_unlock(&race.gate) == 0);
        if (round % 3U == 2U) {
            surface_service_close_owner(2U); /* Race all three operations for the service lock. */
        }
        assert(pthread_join(writer, NULL) == 0 && pthread_join(reader, NULL) == 0);
        if (round % 3U == 1U) {
            assert(race.committed == 0 && race.read == 0);
            surface_service_close_owner(2U); /* Deterministic service-before-teardown ordering. */
        } else if (round % 3U == 0U) {
            assert(race.committed == -TABOS_EBADF && race.read == -TABOS_EBADF);
        }
        assert(race.committed == 0 || race.committed == -TABOS_EBADF);
        assert(race.read == 0 || race.read == -TABOS_EBADF);
        if (race.read == 0) {
            assert(race.pixels[0] == 0U || race.pixels[0] == 0x1234U);
            for (size_t index = 1U; index < 16U; ++index) {
                assert(race.pixels[index] == race.pixels[0]);
            }
        }
        assert(live_allocations == 0U);
        assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &race.packet, frame, sizeof(frame)) == -TABOS_EBADF);
        surface_transport_packet_t fresh = {.width = 4U, .height = 4U};
        fresh.surface                    = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &fresh, NULL, 0U);
        assert(fresh.surface > 0 && fresh.surface != race.packet.surface);
        assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &fresh, frame, sizeof(frame)) == -TABOS_EBADF);
        assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &race.packet, frame, sizeof(frame)) == -TABOS_EBADF);
        surface_service_close_owner(2U);
        assert(surface_service_request(2U, SURFACE_TRANSPORT_STATS, &fresh, NULL, 0U) == 0);
        assert(fresh.stats.used_bytes == 0U && live_allocations == 0U);
        assert(pthread_cond_destroy(&race.changed) == 0 && pthread_mutex_destroy(&race.gate) == 0);
    }
    surface_service_shutdown();
    ipc_service_shutdown();
}

int main(void)
{
    allocation_failures();
    invalid_transfers();
    teardown_races();
    assert(ipc_service_init() && surface_service_init());
    surface_transport_packet_t packet = {.width = 4U, .height = 3U};
    const int surface                 = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &packet, NULL, 0U);
    assert(surface > 0);
    packet.surface = surface;
    uint16_t pixels[12];
    for (unsigned int index = 0U; index < 12U; ++index) {
        pixels[index] = (uint16_t) (index + 1U);
    }
    assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, pixels, sizeof(pixels)) == 0);
    uint16_t result[12];
    memset(result, 0xff, sizeof(result));
    assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == 0);
    for (unsigned int index = 0U; index < 12U; ++index) {
        assert(result[index] == 0U);
    }
    assert(operation(3U, SURFACE_TRANSPORT_COMMIT, surface) == -TABOS_EBADF);
    assert(operation(2U, SURFACE_TRANSPORT_COMMIT, surface) == 0);
    assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == 0);
    assert(memcmp(result, pixels, sizeof(result)) == 0);
    assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == -TABOS_EBADF);

    ipc_transport_packet_t connection = {0};
    const int listener                = ipc_service_request(1U, 1U, IPC_TRANSPORT_LISTEN, &connection);
    const int channel                 = ipc_service_request(2U, 1U, IPC_TRANSPORT_CONNECT, &connection);
    assert(listener > 0 && channel > 0);
    packet.channel = channel;
    assert(surface_service_request(2U, SURFACE_TRANSPORT_GRANT, &packet, NULL, 0U) == 0);
    assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == 0);
    assert(operation(1U, SURFACE_TRANSPORT_RELEASE, surface) == -TABOS_EBADF);
    assert(operation(1U, SURFACE_TRANSPORT_COMMIT, surface) == -TABOS_EBADF);

    packet.x          = 1U;
    packet.y          = 1U;
    packet.width      = 2U;
    packet.height     = 1U;
    uint16_t patch[2] = {0x1234U, 0x5678U};
    assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, patch, sizeof(patch)) == 0);
    packet.x = UINT32_MAX;
    assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, patch, sizeof(patch)) == -TABOS_EINVAL);
    assert(operation(2U, SURFACE_TRANSPORT_COMMIT, surface) == 0);
    packet.x      = 0U;
    packet.y      = 0U;
    packet.width  = 4U;
    packet.height = 3U;
    assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == 0);
    assert(memcmp(result, pixels, sizeof(result)) == 0);
    assert(surface_service_request(2U, SURFACE_TRANSPORT_INFO, &packet, NULL, 0U) == 0 && packet.info.revision == 1U);
    surface_service_close_owner(1U);
    assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == -TABOS_EBADF);
    surface_service_close_owner(2U);
    assert(operation(2U, SURFACE_TRANSPORT_COMMIT, surface) == -TABOS_EBADF);

    /* Failed staging allocation at the per-owner budget retains committed image. */
    surface_transport_packet_t large = {.width = 1280U, .height = 720U};
    int allocated[16];
    unsigned int count = 0U;
    while (count < 16U) {
        const int handle = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &large, NULL, 0U);
        if (handle < 0) {
            assert(handle == -TABOS_ENOMEM);
            break;
        }
        allocated[count++] = handle;
    }
    assert(count > 0U && count < 16U);
    packet = (surface_transport_packet_t) {.surface = allocated[0], .width = 1U, .height = 1U};
    assert(surface_service_request(2U, SURFACE_TRANSPORT_UPLOAD, &packet, patch, sizeof(patch)) == -TABOS_ENOMEM);
    assert(surface_service_request(2U, SURFACE_TRANSPORT_READ, &packet, result, sizeof(result)) == 0 &&
           result[0] == 0U);
    surface_service_close_owner(2U);
    assert(surface_service_request(2U, SURFACE_TRANSPORT_STATS, &packet, NULL, 0U) == 0);
    assert(packet.stats.used_bytes == 0U && packet.stats.peak_bytes <= packet.stats.limit_bytes);

    packet             = (surface_transport_packet_t) {.width = 64U, .height = 64U, .channel = channel};
    concurrent_surface = surface_service_request(2U, SURFACE_TRANSPORT_CREATE, &packet, NULL, 0U);
    packet.surface     = concurrent_surface;
    assert(concurrent_surface > 0 && surface_service_request(2U, SURFACE_TRANSPORT_GRANT, &packet, NULL, 0U) == 0);
    pthread_t writer;
    assert(pthread_create(&writer, NULL, publish_frames, NULL) == 0);
    uint16_t copied[64U * 64U];
    do {
        assert(surface_service_request(1U, SURFACE_TRANSPORT_READ, &packet, copied, sizeof(copied)) == 0);
        for (unsigned int index = 1U; index < 64U * 64U; ++index) {
            assert(copied[index] == copied[0]);
        }
    } while (!atomic_load_explicit(&writer_done, memory_order_acquire));
    assert(pthread_join(writer, NULL) == 0);
    surface_service_close_owner(2U);
    ipc_service_shutdown();
    surface_service_shutdown();
    assert(live_allocations == 0U);
    return 0;
}
