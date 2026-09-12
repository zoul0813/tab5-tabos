#ifndef TABOS_INTERNAL_SURFACE_TRANSPORT_H
#define TABOS_INTERNAL_SURFACE_TRANSPORT_H
#include <tabos/surface.h>

enum {
    SURFACE_TRANSPORT_CREATE,
    SURFACE_TRANSPORT_RELEASE,
    SURFACE_TRANSPORT_GRANT,
    SURFACE_TRANSPORT_INFO,
    SURFACE_TRANSPORT_UPLOAD,
    SURFACE_TRANSPORT_COMMIT,
    SURFACE_TRANSPORT_ABORT,
    SURFACE_TRANSPORT_READ,
    SURFACE_TRANSPORT_STATS,
};

typedef struct {
        int32_t surface;
        int32_t channel;
        uint32_t x, y, width, height;
        tabos_surface_info_t info;
        tabos_surface_stats_t stats;
} surface_transport_packet_t;
#endif
