#include <tabos/platform/platform.h>

#include <SDL3/SDL.h>

#include <limits.h>
#include <string.h>

#include "camera_fixtures.inc"

enum {
    HOST_CAMERA_WIDTH  = 64,
    HOST_CAMERA_HEIGHT = 48,
    HOST_CAMERA_FPS    = 10,
};

static platform_camera_frame_fn frame_callback;
static platform_camera_error_fn error_callback;
static platform_camera_capture_ready_fn capture_ready_callback;
static tabos_camera_config_t active_config;
static SDL_Mutex* worker_mutex;
static SDL_Condition* worker_condition;
static SDL_Thread* worker_thread;
static uint64_t next_frame_ms;
static uint64_t wake_generation;
static uint32_t frame_sequence;
static bool worker_shutdown;
static bool worker_processing;
static bool streaming;

static void submit_fixture(const tabos_camera_config_t* config, uint32_t sequence, uint64_t timestamp_ms)
{
    uint8_t fixture[HOST_CAMERA_WIDTH * HOST_CAMERA_HEIGHT * 2U];
    if (config->format == TABOS_CAMERA_FORMAT_JPEG) {
        frame_callback(host_camera_jpeg, host_camera_jpeg_len, config->width, config->height, 0U,
                       TABOS_CAMERA_FORMAT_JPEG, timestamp_ms);
        return;
    }
    if (config->format == TABOS_CAMERA_FORMAT_H264) {
        frame_callback(host_camera_h264, host_camera_h264_len, config->width, config->height, 0U,
                       TABOS_CAMERA_FORMAT_H264, timestamp_ms);
        return;
    }
    for (uint32_t y = 0U; y < config->height; ++y) {
        for (uint32_t x = 0U; x < config->width; ++x) {
            const uint8_t gray = (uint8_t) (x + y + sequence);
            if (config->format == TABOS_CAMERA_FORMAT_RGB565) {
                const uint16_t pixel =
                    (uint16_t) (((uint16_t) (gray & 0xf8U) << 8U) | ((uint16_t) (gray & 0xfcU) << 3U) | (gray >> 3U));
                const size_t offset  = ((size_t) y * config->width + x) * 2U;
                fixture[offset]      = (uint8_t) pixel;
                fixture[offset + 1U] = (uint8_t) (pixel >> 8U);
            } else {
                fixture[y * config->width + x] = gray;
            }
        }
    }
    const uint32_t pixel_bytes = config->format == TABOS_CAMERA_FORMAT_RGB565 ? 2U : 1U;
    frame_callback(fixture, config->width * config->height * pixel_bytes, config->width, config->height,
                   config->width * pixel_bytes, config->format, timestamp_ms);
}

static int camera_worker(void* unused)
{
    (void) unused;
    SDL_LockMutex(worker_mutex);
    while (!worker_shutdown) {
        while (!worker_shutdown && !streaming) {
            SDL_WaitCondition(worker_condition, worker_mutex);
        }
        if (worker_shutdown) {
            break;
        }
        const uint64_t now = platform_time_ms();
        if (now < next_frame_ms) {
            const uint64_t remaining = next_frame_ms - now;
            const Sint32 timeout_ms  = remaining > INT32_MAX ? INT32_MAX : (Sint32) remaining;
            (void) SDL_WaitConditionTimeout(worker_condition, worker_mutex, timeout_ms);
            continue;
        }
        const tabos_camera_config_t config = active_config;
        const uint64_t observed_generation = wake_generation;
        worker_processing                  = true;
        SDL_UnlockMutex(worker_mutex);
        const bool ready = capture_ready_callback();
        if (ready) {
            submit_fixture(&config, frame_sequence, now);
        }
        SDL_LockMutex(worker_mutex);
        worker_processing = false;
        SDL_BroadcastCondition(worker_condition);
        if (!streaming || worker_shutdown) {
            continue;
        }
        if (!ready) {
            if (observed_generation == wake_generation) {
                SDL_WaitCondition(worker_condition, worker_mutex);
            }
            continue;
        }
        ++frame_sequence;
        next_frame_ms = now + 1000U / config.fps;
    }
    worker_processing = false;
    SDL_BroadcastCondition(worker_condition);
    SDL_UnlockMutex(worker_mutex);
    return 0;
}

bool platform_camera_init(platform_camera_frame_fn frame, platform_camera_error_fn error,
                          platform_camera_capture_ready_fn capture_ready, platform_camera_info_t* info)
{
    if (frame == NULL || error == NULL || capture_ready == NULL || info == NULL) {
        return false;
    }
    worker_mutex = SDL_CreateMutex();
    if (worker_mutex == NULL) {
        return false;
    }
    worker_condition = SDL_CreateCondition();
    if (worker_condition == NULL) {
        SDL_DestroyMutex(worker_mutex);
        worker_mutex = NULL;
        return false;
    }
    frame_callback         = frame;
    error_callback         = error;
    capture_ready_callback = capture_ready;
    worker_thread          = SDL_CreateThread(camera_worker, "tabos-camera", NULL);
    if (worker_thread == NULL) {
        SDL_DestroyCondition(worker_condition);
        SDL_DestroyMutex(worker_mutex);
        worker_condition       = NULL;
        worker_mutex           = NULL;
        frame_callback         = NULL;
        error_callback         = NULL;
        capture_ready_callback = NULL;
        return false;
    }
    *info = (platform_camera_info_t) {.driver  = "host-fixture",
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
    SDL_LockMutex(worker_mutex);
    active_config  = *config;
    next_frame_ms  = platform_time_ms();
    frame_sequence = 0U;
    streaming      = true;
    ++wake_generation;
    SDL_BroadcastCondition(worker_condition);
    SDL_UnlockMutex(worker_mutex);
    return true;
}

void platform_camera_stop(void)
{
    if (worker_mutex == NULL) {
        return;
    }
    SDL_LockMutex(worker_mutex);
    streaming = false;
    ++wake_generation;
    SDL_BroadcastCondition(worker_condition);
    while (worker_processing) {
        SDL_WaitCondition(worker_condition, worker_mutex);
    }
    SDL_UnlockMutex(worker_mutex);
}

void platform_camera_resume(void)
{
    if (worker_mutex == NULL) {
        return;
    }
    SDL_LockMutex(worker_mutex);
    ++wake_generation;
    SDL_SignalCondition(worker_condition);
    SDL_UnlockMutex(worker_mutex);
}

void platform_camera_shutdown(void)
{
    platform_camera_stop();
    if (worker_thread != NULL) {
        SDL_LockMutex(worker_mutex);
        worker_shutdown = true;
        ++wake_generation;
        SDL_BroadcastCondition(worker_condition);
        SDL_UnlockMutex(worker_mutex);
        SDL_WaitThread(worker_thread, NULL);
        worker_thread = NULL;
    }
    if (worker_condition != NULL) {
        SDL_DestroyCondition(worker_condition);
        worker_condition = NULL;
    }
    if (worker_mutex != NULL) {
        SDL_DestroyMutex(worker_mutex);
        worker_mutex = NULL;
    }
    frame_callback         = NULL;
    error_callback         = NULL;
    capture_ready_callback = NULL;
    active_config          = (tabos_camera_config_t) {0};
    next_frame_ms          = 0U;
    wake_generation        = 0U;
    frame_sequence         = 0U;
    worker_shutdown        = false;
    worker_processing      = false;
    streaming              = false;
}
