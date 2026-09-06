#include <tester/test.h>

#include <tabos/audio.h>
#include <tabos/camera.h>
#include <tabos/device.h>
#include <tabos/network.h>
#include <tabos/runtime_time.h>
#include <tabos/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    SERVICE_ROUNDS     = 6,
    SERVICE_BYTES      = 4096,
    SERVICE_TIMEOUT_MS = 5000
};

typedef struct {
        int16_t silence[SERVICE_BYTES / sizeof(int16_t)];
        uint8_t captured[SERVICE_BYTES];
        uint8_t readback[SERVICE_BYTES];
} service_buffers_t;

static bool audio_drained(tabos_audio_stream_t audio)
{
    const uint64_t deadline = tabos_monotonic_ms() + SERVICE_TIMEOUT_MS;
    do {
        tabos_audio_status_t status;
        if (tabos_audio_get_status(audio, &status) != 0) {
            return false;
        }
        if (status.buffered_bytes == 0U) {
            return true;
        }
        (void) tabos_sleep_ms(10U);
    } while (tabos_monotonic_ms() < deadline);
    return false;
}

void tester_test_camera_services(tester_context_t* context)
{
    tabos_device_info_t device;
    tabos_camera_info_t info;
    const bool available = tabos_device_find(TABOS_DEVICE_NAME_CAMERA, &device) == 0 &&
                           device.state == TABOS_DEVICE_READY && tabos_camera_get_info(device.id, &info) == 0 &&
                           (info.formats & TABOS_CAMERA_FORMAT_FLAG_RGB565) != 0U;
    tester_expect(context, available, "service test requires ready RGB565 camera");
    if (!available) {
        return;
    }

    // Keep bulk buffers off the native task stack, which also carries service calls.
    service_buffers_t* buffers = calloc(1U, sizeof(*buffers));
    tester_expect(context, buffers != NULL, "service buffers allocate on application heap");
    if (buffers == NULL) {
        return;
    }
    // Exclusive creation protects any existing file, including a prior failed run.
    const char* path = "T:/tabos-camera-services.tmp";
    const int file   = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    tester_expect(context, file >= 0, "service test creates exclusive scratch file");
    if (file < 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        free(buffers);
        return;
    }
    const tabos_audio_config_t audio_config = {
        .direction = TABOS_AUDIO_PLAYBACK, .channels = 2U, .route = TABOS_AUDIO_ROUTE_SPEAKER};
    const tabos_audio_stream_t audio = tabos_audio_open(&audio_config);
    const tabos_socket_t receiver    = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    const tabos_socket_t sender      = tabos_socket_open(TABOS_NETWORK_FAMILY_IPV4, TABOS_SOCKET_UDP);
    tabos_socket_endpoint_t endpoint = {
        .address = {.family = TABOS_NETWORK_FAMILY_IPV4, .text = "127.0.0.1"}
    };
    const bool sockets = receiver >= 0 && sender >= 0 && tabos_socket_bind(receiver, &endpoint) == 0 &&
                         tabos_socket_get_local_endpoint(receiver, &endpoint) == 0 && endpoint.port != 0U &&
                         tabos_socket_set_nonblocking(receiver, true) == 0 &&
                         tabos_socket_set_nonblocking(sender, true) == 0;
    const tabos_camera_config_t config = {.device_id = device.id,
                                          .format    = TABOS_CAMERA_FORMAT_RGB565,
                                          .width     = info.max_width < 1280U ? info.max_width : 1280U,
                                          .height    = info.max_height < 720U ? info.max_height : 720U,
                                          .fps       = info.max_fps < 30U ? info.max_fps : 30U};
    const tabos_camera_stream_t camera = tabos_camera_open(&config);
    tester_expect(context, audio != TABOS_AUDIO_STREAM_INVALID, "audio opens alongside camera");
    tester_expect(context, sockets, "UDP loopback opens alongside camera");
    tester_expect(context, camera != TABOS_CAMERA_STREAM_INVALID, "camera opens alongside audio and sockets");
    unsigned int completed     = 0U;
    uint32_t previous_sequence = 0U;
    uint32_t drops             = 0U;
    uint64_t longest_round     = 0U;
    const uint64_t started     = tabos_monotonic_ms();
    if (audio != TABOS_AUDIO_STREAM_INVALID && sockets && camera != TABOS_CAMERA_STREAM_INVALID) {
        tabos_wait_item_t camera_wait = {.source = tabos_camera_wait_source(camera),
                                         .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP};
        tabos_wait_item_t socket_wait = {.source = tabos_socket_wait_source(receiver),
                                         .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP};
        for (unsigned int round = 0U; round < SERVICE_ROUNDS; ++round) {
            const uint64_t round_started = tabos_monotonic_ms();
            const bool audio_queued =
                tabos_audio_write(audio, buffers->silence, sizeof(buffers->silence)) == sizeof(buffers->silence);
            const uint32_t token = round + 1U;
            const bool sent      = tabos_socket_send_to(sender, &token, sizeof(token), &endpoint) == sizeof(token);
            tabos_camera_frame_t frame = {0};
            const bool acquired        = tabos_wait(&camera_wait, 1U, SERVICE_TIMEOUT_MS) == 1 &&
                                  camera_wait.returned_events == TABOS_WAIT_READABLE &&
                                  tabos_camera_acquire(camera, &frame) == 0;
            tester_expect(context, acquired && frame.sequence > previous_sequence,
                          "camera advances with audio and UDP pending");
            if (!acquired) {
                break;
            }
            previous_sequence    = frame.sequence;
            drops                = frame.dropped_frames;
            const uint32_t bytes = frame.size_bytes < SERVICE_BYTES ? frame.size_bytes : SERVICE_BYTES;
            const bool stored    = bytes > 0U &&
                                tabos_camera_copy(camera, frame.lease, 0U, buffers->captured, bytes) == (int) bytes &&
                                lseek(file, 0, SEEK_SET) == 0 && write(file, buffers->captured, bytes) == (int) bytes &&
                                lseek(file, 0, SEEK_SET) == 0 && read(file, buffers->readback, bytes) == (int) bytes &&
                                memcmp(buffers->captured, buffers->readback, bytes) == 0;
            tester_expect(context, stored, "frame bytes survive file write/readback while capture runs");
            uint32_t received    = 0U;
            const bool delivered = sent && tabos_wait(&socket_wait, 1U, SERVICE_TIMEOUT_MS) == 1 &&
                                   socket_wait.returned_events == TABOS_WAIT_READABLE &&
                                   tabos_socket_receive(receiver, &received, sizeof(received)) == sizeof(received) &&
                                   received == token;
            tester_expect(context, delivered, "UDP payload delivered while camera lease held");
            tester_expect(context, audio_queued && audio_drained(audio), "queued audio drains while camera runs");
            tester_expect(context, tabos_camera_release(camera, frame.lease) == 0, "concurrent-service lease released");
            const uint64_t elapsed = tabos_monotonic_ms() - round_started;
            if (elapsed > longest_round) {
                longest_round = elapsed;
            }
            ++completed;
        }
    }
    tester_expect(context, completed == SERVICE_ROUNDS, "all overlapping service rounds complete");
    printf("Camera services: rounds=%u elapsed_ms=%" PRIu64 " max_round_ms=%" PRIu64 " drops=%" PRIu32 "\n", completed,
           tabos_monotonic_ms() - started, longest_round, drops);
    if (camera != TABOS_CAMERA_STREAM_INVALID) {
        tester_expect(context, tabos_camera_close(camera) == 0, "service camera closes");
    }
    if (audio != TABOS_AUDIO_STREAM_INVALID) {
        tester_expect(context, tabos_audio_close(audio) == 0, "service audio closes");
    }
    if (receiver >= 0) {
        tester_expect(context, tabos_socket_close(receiver) == 0, "service receiver closes");
    }
    if (sender >= 0) {
        tester_expect(context, tabos_socket_close(sender) == 0, "service sender closes");
    }
    tester_expect(context, close(file) == 0, "service scratch file closes");
    tester_expect(context, unlink(path) == 0, "service scratch file removed");
    free(buffers);
}
