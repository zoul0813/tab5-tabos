#include <tester/test.h>

#include <tabos/camera.h>
#include <tabos/device.h>
#include <tabos/wait.h>
#include <tabos/process.h>

#include <errno.h>
#include <stdio.h>

static bool cleanup_camera_config(tabos_camera_config_t* config)
{
    tabos_device_info_t device;
    tabos_camera_info_t info;
    if (tabos_device_find(TABOS_DEVICE_NAME_CAMERA, &device) != 0 || device.state != TABOS_DEVICE_READY ||
        tabos_camera_get_info(device.id, &info) != 0) {
        return false;
    }
    *config = (tabos_camera_config_t) {.device_id = device.id,
                                       .format    = TABOS_CAMERA_FORMAT_RAW8,
                                       .width     = info.max_width < 1280U ? info.max_width : 1280U,
                                       .height    = info.max_height < 720U ? info.max_height : 720U,
                                       .fps       = info.max_fps < 30U ? info.max_fps : 30U};
    return true;
}

int tester_camera_leak_fixture(void)
{
    tabos_camera_config_t config;
    if (!cleanup_camera_config(&config)) {
        return 90;
    }
    const tabos_camera_stream_t stream = tabos_camera_open(&config);
    if (stream == TABOS_CAMERA_STREAM_INVALID) {
        return 91;
    }
    tabos_wait_item_t item = {.source = tabos_camera_wait_source(stream),
                              .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP};
    tabos_camera_frame_t frame;
    if (tabos_wait(&item, 1U, 5000U) != 1 || item.returned_events != TABOS_WAIT_READABLE ||
        tabos_camera_acquire(stream, &frame) != 0) {
        return 92;
    }
    // Deliberately leak stream, wait source, and frame lease on a nonzero exit.
    return 89;
}

void tester_test_camera_cleanup(tester_context_t* context)
{
    tabos_camera_config_t config;
    const bool available = cleanup_camera_config(&config);
    tester_expect(context, available, "camera cleanup requires ready camera");
    if (!available) {
        return;
    }
    const char* const arguments[] = {"T:/bin/tester", "--camera-leak"};
    for (unsigned int run = 0U; run < 6U; ++run) {
        const int child_status = tabos_exec(arguments[0], 2, arguments);
        tester_expect(context, child_status == 89, "camera child exits with outstanding lease and wait source");
        if (child_status != 89) {
            printf("Camera cleanup child %u status=%d (expected 89)\n", run + 1U, child_status);
            break;
        }
        const tabos_camera_stream_t stream = tabos_camera_open(&config);
        tester_expect(context, stream != TABOS_CAMERA_STREAM_INVALID, "parent reopens camera after child cleanup");
        if (stream == TABOS_CAMERA_STREAM_INVALID) {
            break;
        }
        tabos_wait_item_t item = {.source = tabos_camera_wait_source(stream),
                                  .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP};
        tabos_camera_frame_t frame;
        uint8_t bytes[16];
        const bool acquired = tabos_wait(&item, 1U, 5000U) == 1 && item.returned_events == TABOS_WAIT_READABLE &&
                              tabos_camera_acquire(stream, &frame) == 0;
        tester_expect(context,
                      acquired && tabos_camera_copy(stream, frame.lease, 0U, bytes, sizeof(bytes)) == sizeof(bytes),
                      "parent receives usable frame after failed child teardown");
        // Closing with a held lease also checks explicit cancellation/reclamation.
        tester_expect(context, tabos_camera_close(stream) == 0, "parent closes camera with outstanding lease");
        tester_expect(context, tabos_wait(&item, 1U, 0U) < 0 && errno == EBADF,
                      "close invalidates parent camera wait source");
    }
}

void tester_test_camera(tester_context_t* context)
{
    tabos_device_info_t device;
    const bool found = tabos_device_find(TABOS_DEVICE_NAME_CAMERA, &device) == 0 &&
                       device.device_class == TABOS_DEVICE_CLASS_CAMERA &&
                       (device.features & TABOS_DEVICE_FEATURE_CAMERA_CAPTURE) != 0U;
    tester_expect(context, found, "camera0 reports capture device");
    if (!found) {
        return;
    }

    tabos_camera_info_t info = {0};
    const bool capabilities  = tabos_camera_get_info(device.id, &info) == 0 && info.device_id == device.id &&
                              (info.formats & TABOS_CAMERA_FORMAT_FLAG_RAW8) != 0U;
    tester_expect(context, capabilities, "camera capabilities use copied metadata");
    if (!capabilities) {
        return;
    }
    const tabos_camera_config_t config = {.device_id = device.id,
                                          .format    = TABOS_CAMERA_FORMAT_RAW8,
                                          .width     = info.max_width < 1280U ? info.max_width : 1280U,
                                          .height    = info.max_height < 720U ? info.max_height : 720U,
                                          .fps       = info.max_fps < 30U ? info.max_fps : 30U};
    const tabos_camera_stream_t stream = tabos_camera_open(&config);
    if (device.state != TABOS_DEVICE_READY) {
        tester_expect(context, stream == TABOS_CAMERA_STREAM_INVALID && errno == ENODEV,
                      "offline camera rejects capture open");
        return;
    }
    const tabos_wait_source_t source = tabos_camera_wait_source(stream);
    tabos_wait_item_t item = {.source = source, .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP};
    tester_expect(context, stream != TABOS_CAMERA_STREAM_INVALID && source != TABOS_WAIT_SOURCE_INVALID,
                  "camera stream and wait source open");
    if (stream == TABOS_CAMERA_STREAM_INVALID || source == TABOS_WAIT_SOURCE_INVALID) {
        (void) tabos_camera_close(stream);
        return;
    }
    tester_expect(context, tabos_wait(&item, 1U, 2000U) == 1 && item.returned_events == TABOS_WAIT_READABLE,
                  "camera frame becomes readable");
    tabos_camera_frame_t frame = {0};
    uint8_t bytes[16];
    tester_expect(context,
                  tabos_camera_acquire(stream, &frame) == 0 && frame.lease != TABOS_CAMERA_LEASE_INVALID &&
                      frame.size_bytes > 0U,
                  "camera acquire returns lease and metadata");
    tester_expect(context, tabos_camera_copy(stream, frame.lease, 0U, bytes, sizeof(bytes)) == sizeof(bytes),
                  "camera lease copies bounded frame bytes");
    tester_expect(context,
                  tabos_camera_release(stream, frame.lease) == 0 && tabos_camera_release(stream, frame.lease) < 0 &&
                      errno == EBADF,
                  "camera release stales lease");
    tester_expect(context, tabos_camera_close(stream) == 0, "camera stream closes");
    errno = 0;
    tester_expect(context, tabos_wait(&item, 1U, 0U) < 0 && errno == EBADF, "closed camera wait source is stale");
    for (unsigned int run = 0U; run < 3U; ++run) {
        const tabos_camera_stream_t reopened = tabos_camera_open(&config);
        tester_expect(context, reopened != TABOS_CAMERA_STREAM_INVALID && reopened != stream,
                      "camera reopens with fresh stream generation");
        if (reopened == TABOS_CAMERA_STREAM_INVALID) {
            break;
        }
        item.source                  = tabos_camera_wait_source(reopened);
        tabos_camera_frame_t held[3] = {0};
        bool full                    = true;
        for (unsigned int index = 0U; index < 3U; ++index) {
            if (tabos_wait(&item, 1U, 2000U) != 1 || tabos_camera_acquire(reopened, &held[index]) != 0) {
                full = false;
                break;
            }
        }
        tester_expect(context, full, "camera can lease complete three-frame pool");
        if (full) {
            tester_expect(context, tabos_wait(&item, 1U, 200U) == 0, "exhausted camera pool has no readable frames");
            tester_expect(context, tabos_camera_acquire(reopened, &frame) < 0 && errno == EAGAIN,
                          "exhausted camera acquire is nonblocking");
            tester_expect(context, tabos_camera_release(reopened, held[0].lease) == 0,
                          "camera releases exhausted slot");
            const bool replacement = tabos_wait(&item, 1U, 2000U) == 1 && tabos_camera_acquire(reopened, &frame) == 0;
            tester_expect(context, replacement && frame.lease != held[0].lease && frame.dropped_frames > 0U,
                          "camera resumes after exhaustion and counts dropped frames");
            tester_expect(context, tabos_camera_release(reopened, held[0].lease) < 0 && errno == EBADF,
                          "old camera lease cannot release reused slot");
        }
        tester_expect(context, tabos_camera_close(reopened) == 0,
                      "camera close reclaims all outstanding leases before repeat launch");
    }
}
