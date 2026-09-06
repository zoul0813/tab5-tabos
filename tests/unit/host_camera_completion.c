#include <tabos/filesystem.h>
#include <tabos/internal/camera.h>
#include <tabos/platform/platform.h>
#include <tabos/wait.h>

#include <SDL3/SDL.h>

#include <stdio.h>

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "host camera completion test failed: %s\n", message);
        ++failures;
    }
}

static bool acquire_until(const void* owner, tabos_camera_stream_t stream, tabos_camera_frame_t* frame,
                          uint32_t timeout_ms)
{
    const uint64_t deadline = SDL_GetTicks() + timeout_ms;
    int result;
    do {
        result = camera_service_acquire(owner, stream, frame);
        if (result != -TABOS_EAGAIN) {
            return result == 0;
        }
        SDL_Delay(2U);
    } while (SDL_GetTicks() < deadline);
    return false;
}

int main(void)
{
    static const int owner;
    expect(platform_init(true), "host platform initializes");
    expect(camera_service_init(), "camera service initializes");
    camera_service_set_device_id(42U);

    const tabos_camera_config_t config = {
        .device_id = 42U, .format = TABOS_CAMERA_FORMAT_H264, .width = 64U, .height = 48U, .fps = 10U};
    const tabos_camera_stream_t stream = camera_service_open(&owner, &config);
    expect(stream > 0, "capture starts");

    tabos_camera_frame_t held[3] = {0};
    for (size_t index = 0U; index < 3U; ++index) {
        expect(acquire_until(&owner, stream, &held[index], 1000U), "worker delivers frame without polling");
        expect(held[index].sequence == index + 1U && held[index].dropped_frames == 0U,
               "encoded sequence remains reference-safe");
    }
    expect((platform_runtime_wait_until(platform_time_ms()) & PLATFORM_RUNTIME_EVENT_CAMERA) != 0U,
           "frame completion wakes runtime");

    SDL_Delay(150U);
    uint32_t events = 0U;
    expect(camera_service_poll(&owner, stream, TABOS_WAIT_ERROR, &events) == 0 && events == 0U,
           "full encoded pool blocks producer without fault");
    expect(camera_service_release(&owner, stream, held[0].lease) == 0, "release restores encoded capacity");
    tabos_camera_frame_t resumed = {0};
    expect(acquire_until(&owner, stream, &resumed, 1000U) && resumed.sequence == 4U,
           "capacity release wakes blocked worker");

    expect(camera_service_close(&owner, stream) == 0, "close interrupts and joins worker capture");
    camera_service_shutdown();
    platform_shutdown();
    return failures == 0 ? 0 : 1;
}
