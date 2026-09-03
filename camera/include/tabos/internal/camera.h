#ifndef TABOS_INTERNAL_CAMERA_H
#define TABOS_INTERNAL_CAMERA_H

#include <tabos/camera.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool camera_service_init(void);
void camera_service_shutdown(void);
bool camera_service_info(tabos_camera_info_t* info, const char** driver, bool* ready, int* error);
void camera_service_set_device_id(tabos_device_id_t device_id);
tabos_camera_stream_t camera_service_open(const void* owner, const tabos_camera_config_t* config);
int camera_service_close(const void* owner, tabos_camera_stream_t stream);
int camera_service_acquire(const void* owner, tabos_camera_stream_t stream, tabos_camera_frame_t* frame);
int camera_service_copy(const void* owner, tabos_camera_stream_t stream, tabos_camera_lease_t lease, uint32_t offset,
                        void* buffer, uint32_t capacity);
int camera_service_release(const void* owner, tabos_camera_stream_t stream, tabos_camera_lease_t lease);
int camera_service_poll(const void* owner, tabos_camera_stream_t stream, uint32_t requested_events,
                        uint32_t* returned_events);
void camera_service_close_owner(const void* owner);
void camera_service_submit(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t stride_bytes,
                           uint32_t format, uint64_t timestamp_ms);
void camera_service_error(int error);
void camera_service_remove_device(void);

#endif
