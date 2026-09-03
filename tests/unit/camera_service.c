#include <tabos/internal/camera.h>
#include <tabos/filesystem.h>
#include <tabos/wait.h>

#include "platform_test.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "camera service test failed: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    static const int owner_a;
    static const int owner_b;
    expect(camera_service_init(), "initializes");
    camera_service_set_device_id(42U);
    tabos_camera_info_t info;
    expect(camera_service_info(&info, NULL, NULL, NULL) && info.device_id == 42U &&
               info.formats == TABOS_CAMERA_FORMAT_FLAG_RAW8,
           "reports copied camera capabilities");
    const tabos_camera_config_t config = {
        .device_id = 42U, .format = TABOS_CAMERA_FORMAT_RAW8, .width = 4U, .height = 2U, .fps = 10U};
    const tabos_camera_stream_t stream = camera_service_open(&owner_a, &config);
    expect(stream != TABOS_CAMERA_STREAM_INVALID, "opens owned stream");
    expect(camera_service_open(&owner_b, &config) == -TABOS_EBUSY, "rejects conflicting stream");

    const uint8_t first[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    test_platform_camera_frame(first, sizeof(first), 4U, 2U, 4U, 100U);
    uint32_t events = 0U;
    expect(camera_service_poll(&owner_a, stream, TABOS_WAIT_READABLE, &events) == 0 && events == TABOS_WAIT_READABLE,
           "frame makes wait source readable");
    tabos_camera_frame_t frame;
    expect(camera_service_acquire(&owner_a, stream, &frame) == 0 && frame.lease != TABOS_CAMERA_LEASE_INVALID &&
               frame.size_bytes == sizeof(first) && frame.timestamp_ms == 100U,
           "acquires opaque frame lease with metadata");
    uint8_t copied[8] = {0};
    expect(camera_service_copy(&owner_a, stream, frame.lease, 0U, copied, sizeof(copied)) == sizeof(copied) &&
               memcmp(copied, first, sizeof(first)) == 0,
           "copies leased bytes without exposing pointer");
    expect(camera_service_release(&owner_b, stream, frame.lease) == -TABOS_EBADF, "rejects foreign owner");
    expect(camera_service_release(&owner_a, stream, frame.lease) == 0 &&
               camera_service_release(&owner_a, stream, frame.lease) == -TABOS_EBADF,
           "release invalidates lease");

    for (uint8_t sequence = 0U; sequence < 6U; ++sequence) {
        uint8_t data[8] = {sequence};
        test_platform_camera_frame(data, sizeof(data), 4U, 2U, 4U, 200U + sequence);
    }
    expect(camera_service_acquire(&owner_a, stream, &frame) == 0 && frame.dropped_frames > 0U,
           "slow consumer drops oldest unleased frames and counts drops");
    uint8_t oldest = 0U;
    expect(camera_service_copy(&owner_a, stream, frame.lease, 0U, &oldest, 1U) == 1 && oldest == 3U,
           "acquire preserves oldest-to-newest order after replacement");
    camera_service_close_owner(&owner_a);
    expect(camera_service_acquire(&owner_a, stream, &frame) == -TABOS_EBADF,
           "process cleanup reclaims leases and stream");
    const tabos_camera_stream_t reused = camera_service_open(&owner_a, &config);
    expect(reused != stream && camera_service_close(&owner_a, stream) == -TABOS_EBADF,
           "generation rejects stale stream after reuse");
    test_platform_camera_error(TABOS_EIO);
    expect(camera_service_poll(&owner_a, reused, TABOS_WAIT_ERROR, &events) == 0 && events == TABOS_WAIT_ERROR,
           "backend fault reports wait error");
    camera_service_remove_device();
    expect(camera_service_poll(&owner_a, reused, TABOS_WAIT_HANGUP, &events) == 0 && events == TABOS_WAIT_HANGUP,
           "device removal reports hangup");
    camera_service_close_owner(&owner_a);
    camera_service_shutdown();
    return failures == 0 ? 0 : 1;
}
