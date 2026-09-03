#include <tabos/platform/platform.h>

#include <string.h>

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
    *info          = (platform_camera_info_t) {.driver     = "host-fixture",
                                               .formats    = TABOS_CAMERA_FORMAT_FLAG_RAW8,
                                               .max_width  = HOST_CAMERA_WIDTH,
                                               .max_height = HOST_CAMERA_HEIGHT,
                                               .max_fps    = HOST_CAMERA_FPS,
                                               .detected   = true,
                                               .ready      = true};
    return true;
}

bool platform_camera_start(const tabos_camera_config_t* config)
{
    if (config == NULL || config->format != TABOS_CAMERA_FORMAT_RAW8 || config->width > HOST_CAMERA_WIDTH ||
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
    uint8_t fixture[HOST_CAMERA_WIDTH * HOST_CAMERA_HEIGHT];
    for (uint32_t y = 0U; y < active_config.height; ++y) {
        for (uint32_t x = 0U; x < active_config.width; ++x) {
            fixture[y * active_config.width + x] = (uint8_t) (x + y + frame_sequence);
        }
    }
    frame_callback(fixture, active_config.width * active_config.height, active_config.width, active_config.height,
                   active_config.width, TABOS_CAMERA_FORMAT_RAW8, now);
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
