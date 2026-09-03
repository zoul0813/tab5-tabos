#include <tabos/platform/platform.h>

#include <bsp/m5stack_tab5.h>
#include <esp_video_init.h>

static bool initialized;

bool platform_camera_init(platform_camera_frame_fn frame, platform_camera_error_fn error, platform_camera_info_t* info)
{
    if (frame == NULL || error == NULL || info == NULL) {
        return false;
    }
    const esp_err_t result = bsp_camera_start(NULL);
    initialized            = result == ESP_OK;
    *info                  = (platform_camera_info_t) {.driver     = "SC2356",
                                                       .formats    = TABOS_CAMERA_FORMAT_FLAG_RAW8,
                                                       .max_width  = 1600U,
                                                       .max_height = 1200U,
                                                       .max_fps    = 30U,
                                                       .detected   = initialized,
                                                       .ready      = false,
                                                       .error      = initialized ? 0 : result};
    return initialized;
}

bool platform_camera_start(const tabos_camera_config_t* config)
{
    (void) config;
    return false;
}

void platform_camera_stop(void)
{
}

void platform_camera_update(void)
{
}

void platform_camera_shutdown(void)
{
    if (initialized) {
        (void) esp_video_deinit();
        (void) bsp_feature_enable(BSP_FEATURE_CAMERA, false);
        initialized = false;
    }
}
