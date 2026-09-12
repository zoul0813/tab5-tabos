#ifndef TABOS_INTERNAL_SURFACE_H
#define TABOS_INTERNAL_SURFACE_H
#include <stdbool.h>
#include <stddef.h>
#include <tabos/internal/surface_transport.h>

bool surface_service_init(void);
void surface_service_shutdown(void);
void surface_service_close_owner(uint32_t owner);
int surface_service_request(uint32_t owner, uint32_t operation, surface_transport_packet_t* packet, void* pixels,
                            size_t pixel_bytes);
#endif
