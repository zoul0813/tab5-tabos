#include <tabos/internal/surface.h>
#include <tabos/internal/ipc.h>
#include <tabos/filesystem.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

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

int main(void)
{
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
    return 0;
}
