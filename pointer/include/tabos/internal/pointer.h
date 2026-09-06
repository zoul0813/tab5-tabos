#ifndef TABOS_INTERNAL_POINTER_H
#define TABOS_INTERNAL_POINTER_H

#include <tabos/pointer.h>

#include <stdbool.h>
#include <stdint.h>

bool pointer_service_init(void);
void pointer_service_shutdown(void);
bool pointer_service_info(const char** driver, int* error);
void pointer_service_set_device_id(tabos_device_id_t device_id);
void pointer_service_set_foreground_owner(const void* owner);
tabos_pointer_stream_t pointer_service_open(const void* owner, tabos_device_id_t device_id);
int pointer_service_close(const void* owner, tabos_pointer_stream_t stream);
int pointer_service_read(const void* owner, tabos_pointer_stream_t stream, tabos_pointer_event_t* event);
int pointer_service_poll(const void* owner, tabos_pointer_stream_t stream, uint32_t requested_events,
                         uint32_t* returned_events);
void pointer_service_close_owner(const void* owner);
void pointer_service_submit(const tabos_pointer_event_t* event);
void pointer_service_remove_device(void);

#endif
