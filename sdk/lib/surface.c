#include <tabos/surface.h>
#include <tabos/internal/elf_api.h>
#include <errno.h>
#include <stddef.h>

extern const tabos_elf_api_t* tabos_runtime_api;

static int call(uint32_t operation, surface_transport_packet_t* packet, void* pixels)
{
    if (tabos_runtime_api == NULL || tabos_runtime_api->surface == NULL) {
        errno = ENOSYS;
        return -1;
    }
    const int result = tabos_runtime_api->surface(operation, packet, pixels);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}

tabos_surface_t tabos_surface_create(uint32_t width, uint32_t height)
{
    surface_transport_packet_t packet = {.width = width, .height = height};
    return call(SURFACE_TRANSPORT_CREATE, &packet, NULL);
}

int tabos_surface_release(tabos_surface_t surface)
{
    surface_transport_packet_t packet = {.surface = surface};
    return call(SURFACE_TRANSPORT_RELEASE, &packet, NULL);
}

int tabos_surface_grant(tabos_surface_t surface, tabos_ipc_channel_t channel)
{
    surface_transport_packet_t packet = {.surface = surface, .channel = channel};
    return call(SURFACE_TRANSPORT_GRANT, &packet, NULL);
}

int tabos_surface_info(tabos_surface_t surface, tabos_surface_info_t* info)
{
    if (info == NULL) {
        errno = EINVAL;
        return -1;
    }
    surface_transport_packet_t packet = {.surface = surface};
    const int result                  = call(SURFACE_TRANSPORT_INFO, &packet, NULL);
    if (result == 0) {
        *info = packet.info;
    }
    return result;
}

int tabos_surface_upload(tabos_surface_t surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                         const uint16_t* pixels)
{
    if (pixels == NULL) {
        (void) tabos_surface_abort(surface);
        errno = EINVAL;
        return -1;
    }
    surface_transport_packet_t packet = {.surface = surface, .x = x, .y = y, .width = width, .height = height};
    const int result                  = call(SURFACE_TRANSPORT_UPLOAD, &packet, (void*) pixels);
    if (result < 0) {
        const int error = errno;
        (void) tabos_surface_abort(surface);
        errno = error;
    }
    return result;
}

int tabos_surface_commit(tabos_surface_t surface)
{
    surface_transport_packet_t packet = {.surface = surface};
    return call(SURFACE_TRANSPORT_COMMIT, &packet, NULL);
}

int tabos_surface_abort(tabos_surface_t surface)
{
    surface_transport_packet_t packet = {.surface = surface};
    return call(SURFACE_TRANSPORT_ABORT, &packet, NULL);
}

int tabos_surface_read(tabos_surface_t surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                       uint16_t* pixels)
{
    if (pixels == NULL) {
        errno = EINVAL;
        return -1;
    }
    surface_transport_packet_t packet = {.surface = surface, .x = x, .y = y, .width = width, .height = height};
    return call(SURFACE_TRANSPORT_READ, &packet, pixels);
}

int tabos_surface_stats(tabos_surface_stats_t* stats)
{
    if (stats == NULL) {
        errno = EINVAL;
        return -1;
    }
    surface_transport_packet_t packet = {0};
    const int result                  = call(SURFACE_TRANSPORT_STATS, &packet, NULL);
    if (result == 0) {
        *stats = packet.stats;
    }
    return result;
}
