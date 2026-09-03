#include <tabos/platform/platform.h>

#include <string.h>

#include "camera_fixtures.inc"

enum {
    HOST_CAMERA_WIDTH  = 64,
    HOST_CAMERA_HEIGHT = 48,
    HOST_CAMERA_FPS    = 10,
};

static platform_camera_frame_fn frame_callback;
static platform_camera_error_fn error_callback;
static tabos_camera_config_t active_config;
static uint64_t next_frame_ms;
static uint32_t frame_sequence;
static bool streaming;

bool platform_camera_init(platform_camera_frame_fn frame, platform_camera_error_fn error, platform_camera_info_t* info)
{
    if (frame == NULL || error == NULL || info == NULL) {
        return false;
    }
    frame_callback = frame;
    error_callback = error;
    *info          = (platform_camera_info_t) {.driver  = "host-fixture",
                                               .formats = TABOS_CAMERA_FORMAT_FLAG_RAW8 | TABOS_CAMERA_FORMAT_FLAG_RGB565 |
                                                          TABOS_CAMERA_FORMAT_FLAG_JPEG | TABOS_CAMERA_FORMAT_FLAG_H264,
                                               .max_width  = HOST_CAMERA_WIDTH,
                                               .max_height = HOST_CAMERA_HEIGHT,
                                               .max_fps    = HOST_CAMERA_FPS,
                                               .detected   = true,
                                               .ready      = true};
    return true;
}

bool platform_camera_start(const tabos_camera_config_t* config)
{
    if (config == NULL || config->format >= TABOS_CAMERA_FORMAT_COUNT || config->width > HOST_CAMERA_WIDTH ||
        config->height > HOST_CAMERA_HEIGHT || config->fps > HOST_CAMERA_FPS) {
        return false;
    }
    active_config  = *config;
    next_frame_ms  = 0U;
    frame_sequence = 0U;
    streaming      = true;
    return true;
}

void platform_camera_stop(void)
{
    streaming = false;
}

void platform_camera_update(void)
{
    if (!streaming || frame_callback == NULL) {
        return;
    }
    const uint64_t now = platform_time_ms();
    if (now < next_frame_ms) {
        return;
    }
    uint8_t fixture[HOST_CAMERA_WIDTH * HOST_CAMERA_HEIGHT * 2U];
    if (active_config.format == TABOS_CAMERA_FORMAT_JPEG) {
        frame_callback(host_camera_jpeg, host_camera_jpeg_len, active_config.width, active_config.height, 0U,
                       TABOS_CAMERA_FORMAT_JPEG, now);
        goto submitted;
    }
    if (active_config.format == TABOS_CAMERA_FORMAT_H264) {
        frame_callback(host_camera_h264, host_camera_h264_len, active_config.width, active_config.height, 0U,
                       TABOS_CAMERA_FORMAT_H264, now);
        goto submitted;
    }
    for (uint32_t y = 0U; y < active_config.height; ++y) {
        for (uint32_t x = 0U; x < active_config.width; ++x) {
            const uint8_t gray = (uint8_t) (x + y + frame_sequence);
            if (active_config.format == TABOS_CAMERA_FORMAT_RGB565) {
                const uint16_t pixel =
                    (uint16_t) (((uint16_t) (gray & 0xf8U) << 8U) | ((uint16_t) (gray & 0xfcU) << 3U) | (gray >> 3U));
                const size_t offset  = ((size_t) y * active_config.width + x) * 2U;
                fixture[offset]      = (uint8_t) pixel;
                fixture[offset + 1U] = (uint8_t) (pixel >> 8U);
            } else {
                fixture[y * active_config.width + x] = gray;
            }
        }
    }
    const uint32_t pixel_bytes = active_config.format == TABOS_CAMERA_FORMAT_RGB565 ? 2U : 1U;
    frame_callback(fixture, active_config.width * active_config.height * pixel_bytes, active_config.width,
                   active_config.height, active_config.width * pixel_bytes, active_config.format, now);
submitted:
    ++frame_sequence;
    next_frame_ms = now + 1000U / active_config.fps;
}

void platform_camera_shutdown(void)
{
    streaming      = false;
    frame_callback = NULL;
    error_callback = NULL;
    active_config  = (tabos_camera_config_t) {0};
}
