#ifndef TABOS_CAMERA_H
#define TABOS_CAMERA_H

#include <tabos/device.h>

#include <stdint.h>

#define TABOS_CAMERA_STREAM_INVALID (-1)
#define TABOS_CAMERA_LEASE_INVALID  0U

typedef int32_t tabos_camera_stream_t;
typedef uint32_t tabos_camera_lease_t;

typedef enum {
    TABOS_CAMERA_FORMAT_RAW8 = 0,
    TABOS_CAMERA_FORMAT_RGB565,
    TABOS_CAMERA_FORMAT_JPEG,
    TABOS_CAMERA_FORMAT_H264,
    TABOS_CAMERA_FORMAT_COUNT,
} tabos_camera_format_t;

enum {
    TABOS_CAMERA_FORMAT_FLAG_RAW8   = 1U << TABOS_CAMERA_FORMAT_RAW8,
    TABOS_CAMERA_FORMAT_FLAG_RGB565 = 1U << TABOS_CAMERA_FORMAT_RGB565,
    TABOS_CAMERA_FORMAT_FLAG_JPEG   = 1U << TABOS_CAMERA_FORMAT_JPEG,
    TABOS_CAMERA_FORMAT_FLAG_H264   = 1U << TABOS_CAMERA_FORMAT_H264,
};

typedef struct {
        tabos_device_id_t device_id;
        uint32_t formats;
        uint32_t max_width;
        uint32_t max_height;
        uint32_t max_fps;
} tabos_camera_info_t;

typedef struct {
        tabos_device_id_t device_id;
        tabos_camera_format_t format;
        uint32_t width;
        uint32_t height;
        uint32_t fps;
} tabos_camera_config_t;

typedef struct {
        tabos_camera_lease_t lease;
        tabos_camera_format_t format;
        uint32_t width;
        uint32_t height;
        uint32_t stride_bytes;
        uint32_t size_bytes;
        uint64_t timestamp_ms;
        uint32_t sequence;
        uint32_t dropped_frames;
} tabos_camera_frame_t;

int tabos_camera_get_info(tabos_device_id_t device_id, tabos_camera_info_t* info);
tabos_camera_stream_t tabos_camera_open(const tabos_camera_config_t* config);
int tabos_camera_close(tabos_camera_stream_t stream);
int tabos_camera_acquire(tabos_camera_stream_t stream, tabos_camera_frame_t* frame);
int tabos_camera_copy(tabos_camera_stream_t stream, tabos_camera_lease_t lease, uint32_t offset, void* buffer,
                      uint32_t capacity);
int tabos_camera_release(tabos_camera_stream_t stream, tabos_camera_lease_t lease);

#endif
