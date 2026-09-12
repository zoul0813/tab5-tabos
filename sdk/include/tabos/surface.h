#ifndef TABOS_SURFACE_H
#define TABOS_SURFACE_H

#include <stdint.h>
#include <tabos/ipc.h>

typedef int32_t tabos_surface_t;
typedef struct {
        uint32_t width;
        uint32_t height;
        uint32_t revision;
} tabos_surface_info_t;

typedef struct {
        uint32_t used_bytes;
        uint32_t peak_bytes;
        uint32_t limit_bytes;
        uint32_t process_limit_bytes;
} tabos_surface_stats_t;

tabos_surface_t tabos_surface_create(uint32_t width, uint32_t height);
int tabos_surface_release(tabos_surface_t surface);
int tabos_surface_grant(tabos_surface_t surface, tabos_ipc_channel_t compositor_channel);
int tabos_surface_info(tabos_surface_t surface, tabos_surface_info_t* info);
int tabos_surface_upload(tabos_surface_t surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                         const uint16_t* pixels);
int tabos_surface_commit(tabos_surface_t surface);
int tabos_surface_abort(tabos_surface_t surface);
int tabos_surface_read(tabos_surface_t surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                       uint16_t* pixels);
int tabos_surface_stats(tabos_surface_stats_t* stats);

#endif
