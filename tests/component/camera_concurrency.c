#include <tabos/internal/camera.h>
#include <tabos/platform/platform.h>
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdio.h>
#include <tabos/filesystem.h>
#include <tabos/wait.h>

static platform_camera_frame_fn submit;
static atomic_int active;
static atomic_int overlaps;
static bool streaming;
static uint8_t encoded_sequence;
static uint32_t active_format;
static const int owner;
static const tabos_camera_config_t config = {
    .device_id = 42U, .format = TABOS_CAMERA_FORMAT_RAW8, .width = 4U, .height = 2U, .fps = 10U};

static void enter_backend(void)
{
    if (atomic_fetch_add(&active, 1) != 0) {
        atomic_fetch_add(&overlaps, 1);
    }
    SDL_Delay(1U);
}

static void leave_backend(void)
{
    atomic_fetch_sub(&active, 1);
}

bool platform_camera_init(platform_camera_frame_fn frame, platform_camera_error_fn error, platform_camera_info_t* info)
{
    (void) error;
    submit = frame;
    *info  = (platform_camera_info_t) {.detected   = true,
                                       .ready      = true,
                                       .formats    = TABOS_CAMERA_FORMAT_FLAG_RAW8 | TABOS_CAMERA_FORMAT_FLAG_H264,
                                       .max_width  = 4U,
                                       .max_height = 2U,
                                       .max_fps    = 10U};
    return true;
}

bool platform_camera_start(const tabos_camera_config_t* requested)
{
    enter_backend();
    streaming        = true;
    active_format    = requested->format;
    encoded_sequence = 0U;
    leave_backend();
    return true;
}

void platform_camera_stop(void)
{
    enter_backend();
    streaming = false;
    leave_backend();
}

void platform_camera_update(void)
{
    enter_backend();
    if (streaming) {
        const uint8_t bytes[8] = {++encoded_sequence};
        submit(bytes, sizeof(bytes), 4U, 2U, 4U, active_format, 1U);
    }
    leave_backend();
}

void platform_camera_shutdown(void)
{
    platform_camera_stop();
}

static int update_worker(void* unused)
{
    (void) unused;
    for (unsigned int index = 0U; index < 100U; ++index) {
        camera_service_update();
    }
    return 0;
}

static int test_encoded_backpressure(void)
{
    tabos_camera_config_t encoded_config = config;
    encoded_config.format                = TABOS_CAMERA_FORMAT_H264;
    const tabos_camera_stream_t stream   = camera_service_open(&owner, &encoded_config);
    if (stream <= 0) {
        return 1;
    }
    // Two update callers fill the pool while the consumer deliberately stalls.
    SDL_Thread* runtime = SDL_CreateThread(update_worker, "encoded runtime", NULL);
    SDL_Thread* waiter  = SDL_CreateThread(update_worker, "encoded waiter", NULL);
    if (runtime != NULL) {
        SDL_WaitThread(runtime, NULL);
    }
    if (waiter != NULL) {
        SDL_WaitThread(waiter, NULL);
    }
    int failures                 = runtime == NULL || waiter == NULL || encoded_sequence != 3U;
    tabos_camera_frame_t held[3] = {0};
    for (size_t index = 0U; index < 3U; ++index) {
        uint8_t byte = 0U;
        if (camera_service_acquire(&owner, stream, &held[index]) != 0 ||
            camera_service_copy(&owner, stream, held[index].lease, 0U, &byte, 1U) != 1 || byte != index + 1U ||
            held[index].dropped_frames != 0U) {
            ++failures;
        }
    }
    camera_service_update();
    if (encoded_sequence != 3U) {
        ++failures;
    }
    // Acquiring does not free a slot; only release permits another encoded picture.
    for (size_t index = 0U; index < 3U; ++index) {
        (void) camera_service_release(&owner, stream, held[index].lease);
        camera_service_update();
    }
    for (size_t index = 0U; index < 3U; ++index) {
        uint8_t byte = 0U;
        if (camera_service_acquire(&owner, stream, &held[index]) != 0 ||
            camera_service_copy(&owner, stream, held[index].lease, 0U, &byte, 1U) != 1 || byte != index + 4U ||
            held[index].dropped_frames != 0U) {
            ++failures;
        }
    }
    // An unexpected asynchronous submission must fault rather than lose a reference.
    const uint8_t extra[8] = {7U};
    submit(extra, sizeof(extra), 4U, 2U, 0U, TABOS_CAMERA_FORMAT_H264, 1U);
    uint32_t events = 0U;
    if (camera_service_poll(&owner, stream, TABOS_WAIT_ERROR, &events) != 0 || events != TABOS_WAIT_ERROR) {
        ++failures;
    }
    camera_service_close_owner(&owner);
    const tabos_camera_stream_t reopened = camera_service_open(&owner, &encoded_config);
    if (reopened <= 0) {
        return failures + 1;
    }
    const uint8_t oversized[17] = {0};
    submit(oversized, sizeof(oversized), 4U, 2U, 0U, TABOS_CAMERA_FORMAT_H264, 1U);
    if (camera_service_acquire(&owner, reopened, &held[0]) != -TABOS_EIO) {
        ++failures;
    }
    camera_service_close_owner(&owner);
    if (failures != 0) {
        fputs("encoded pool backpressure/reference preservation failed\n", stderr);
    }
    return failures;
}

int main(void)
{
    if (!camera_service_init()) {
        return 1;
    }
    camera_service_set_device_id(42U);
    SDL_Thread* runtime = SDL_CreateThread(update_worker, "camera runtime", NULL);
    SDL_Thread* waiter  = SDL_CreateThread(update_worker, "camera wait", NULL);
    if (runtime == NULL || waiter == NULL) {
        if (runtime != NULL) {
            SDL_WaitThread(runtime, NULL);
        }
        if (waiter != NULL) {
            SDL_WaitThread(waiter, NULL);
        }
        camera_service_shutdown();
        return 1;
    }
    int failures = 0;
    for (unsigned int index = 0U; index < 30U; ++index) {
        const tabos_camera_stream_t stream = camera_service_open(&owner, &config);
        if (stream <= 0) {
            ++failures;
            break;
        }
        camera_service_update();
        tabos_camera_frame_t frame = {0};
        if (camera_service_acquire(&owner, stream, &frame) != 0) {
            ++failures;
        }
        if ((index & 1U) == 0U) {
            if (camera_service_close(&owner, stream) != 0) {
                ++failures;
            }
        } else {
            camera_service_close_owner(&owner);
        }
    }
    SDL_WaitThread(runtime, NULL);
    SDL_WaitThread(waiter, NULL);
    failures += test_encoded_backpressure();
    camera_service_shutdown();
    if (atomic_load(&overlaps) != 0) {
        fputs("camera backend update/start/stop overlapped\n", stderr);
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
