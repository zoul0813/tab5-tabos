#include <tabos/camera.h>
#include <tabos/device.h>
#include <tabos/graphics.h>
#include <tabos/input.h>
#include <tabos/wait.h>
#include <tabos/runtime_time.h>

#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CAMERA_WIDTH      = 1280,
    CAMERA_HEIGHT     = 720,
    CAMERA_FPS        = 30,
    FRAME_TIMEOUT_MS  = 2000,
    COPY_BYTES        = 4096,
    WRITE_YIELD_BYTES = 65536,
    STILL_SETTLE_MS   = 2000,
    STILL_MIN_FRAMES  = 6,
    STILL_TIMEOUT_MS  = 10000,
};

static void usage(FILE* output)
{
    fputs(
        "Usage: cameratest [info|capture raw|rgb|jpeg PATH|record h264 PATH [FRAMES]|preview [FRAMES]|snapshot PATH]\n",
        output);
}

static const char* state_name(tabos_device_state_t state)
{
    switch (state) {
        case TABOS_DEVICE_READY: return "ready";
        case TABOS_DEVICE_OFFLINE: return "offline";
        case TABOS_DEVICE_FAULT: return "fault";
        case TABOS_DEVICE_STATE_COUNT: break;
    }
    return "unknown";
}

static int camera_info(tabos_device_info_t* device, tabos_camera_info_t* info)
{
    if (tabos_device_find(TABOS_DEVICE_NAME_CAMERA, device) != 0 || tabos_camera_get_info(device->id, info) != 0) {
        fprintf(stderr, "cameratest: camera0 unavailable: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int show_info(void)
{
    tabos_device_info_t device;
    tabos_camera_info_t info;
    if (camera_info(&device, &info) != 0) {
        return 1;
    }
    printf("camera0: %s; driver=%s; max=%lux%lu@%lu; formats=", state_name(device.state), device.driver,
           (unsigned long) info.max_width, (unsigned long) info.max_height, (unsigned long) info.max_fps);
    if ((info.formats & TABOS_CAMERA_FORMAT_FLAG_RAW8) != 0U) {
        fputs(" raw8", stdout);
    }
    if ((info.formats & TABOS_CAMERA_FORMAT_FLAG_RGB565) != 0U) {
        fputs(" rgb565", stdout);
    }
    if ((info.formats & TABOS_CAMERA_FORMAT_FLAG_JPEG) != 0U) {
        fputs(" jpeg", stdout);
    }
    if ((info.formats & TABOS_CAMERA_FORMAT_FLAG_H264) != 0U) {
        fputs(" h264", stdout);
    }
    printf("; error=%ld\n", (long) device.last_error);
    return 0;
}

static tabos_camera_stream_t camera_open(tabos_camera_format_t format, tabos_camera_config_t* opened_config)
{
    tabos_device_info_t device;
    tabos_camera_info_t info;
    if (camera_info(&device, &info) != 0) {
        return TABOS_CAMERA_STREAM_INVALID;
    }
    if (device.state != TABOS_DEVICE_READY) {
        fprintf(stderr, "cameratest: camera0 is %s\n", state_name(device.state));
        errno = ENODEV;
        return TABOS_CAMERA_STREAM_INVALID;
    }
    tabos_camera_config_t config = {
        .device_id = device.id,
        .format    = format,
        .width     = info.max_width < CAMERA_WIDTH ? info.max_width : CAMERA_WIDTH,
        .height    = info.max_height < CAMERA_HEIGHT ? info.max_height : CAMERA_HEIGHT,
        .fps       = info.max_fps < CAMERA_FPS ? info.max_fps : CAMERA_FPS,
    };
    const tabos_camera_stream_t stream = tabos_camera_open(&config);
    if (stream == TABOS_CAMERA_STREAM_INVALID) {
        fprintf(stderr, "cameratest: open failed: %s\n", strerror(errno));
    }
    if (stream != TABOS_CAMERA_STREAM_INVALID && opened_config != NULL) {
        *opened_config = config;
    }
    return stream;
}

static int acquire(tabos_camera_stream_t stream, tabos_camera_frame_t* frame)
{
    tabos_wait_item_t item = {
        .source = tabos_camera_wait_source(stream),
        .events = TABOS_WAIT_READABLE | TABOS_WAIT_ERROR | TABOS_WAIT_HANGUP,
    };
    const int ready = tabos_wait(&item, 1U, FRAME_TIMEOUT_MS);
    if (ready <= 0 || (item.returned_events & TABOS_WAIT_READABLE) == 0U) {
        if (ready == 0) {
            errno = ETIMEDOUT;
        } else if ((item.returned_events & TABOS_WAIT_HANGUP) != 0U) {
            errno = ENODEV;
        } else if ((item.returned_events & TABOS_WAIT_ERROR) != 0U) {
            errno = EIO;
        }
        return -1;
    }
    return tabos_camera_acquire(stream, frame);
}

static int write_frame(tabos_camera_stream_t stream, const tabos_camera_frame_t* frame, FILE* output)
{
    uint8_t bytes[COPY_BYTES];
    uint32_t offset            = 0U;
    uint32_t bytes_since_yield = 0U;
    while (offset < frame->size_bytes) {
        uint32_t capacity = frame->size_bytes - offset;
        if (capacity > sizeof(bytes)) {
            capacity = sizeof(bytes);
        }
        const int copied = tabos_camera_copy(stream, frame->lease, offset, bytes, capacity);
        if (copied <= 0 || fwrite(bytes, 1U, (size_t) copied, output) != (size_t) copied) {
            return -1;
        }
        offset            += (uint32_t) copied;
        bytes_since_yield += (uint32_t) copied;
        if (bytes_since_yield >= WRITE_YIELD_BYTES) {
            if (sched_yield() != 0) {
                return -1;
            }
            bytes_since_yield = 0U;
        }
    }
    return 0;
}

static int acquire_still(tabos_camera_stream_t stream, tabos_camera_frame_t* frame)
{
    const uint64_t started = tabos_monotonic_ms();
    unsigned int frames    = 0U;
    puts("cameratest: allowing exposure and white balance to settle...");
    for (;;) {
        if (acquire(stream, frame) != 0) {
            return -1;
        }
        ++frames;
        // Use capture time, not file-writing time or time spent waiting behind a
        // queued frame. Never apply this discard policy to dependent H.264 pictures.
        if (frames >= STILL_MIN_FRAMES && frame->timestamp_ms >= started &&
            frame->timestamp_ms - started >= STILL_SETTLE_MS) {
            return 0;
        }
        if (tabos_camera_release(stream, frame->lease) != 0) {
            return -1;
        }
        if (tabos_monotonic_ms() - started >= STILL_TIMEOUT_MS) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (sched_yield() != 0) {
            return -1;
        }
    }
}

static int capture(tabos_camera_format_t format, const char* path)
{
    const tabos_camera_stream_t stream = camera_open(format, NULL);
    if (stream == TABOS_CAMERA_STREAM_INVALID) {
        return 1;
    }
    tabos_camera_frame_t frame;
    int status = 1;
    if (acquire_still(stream, &frame) == 0) {
        FILE* output = fopen(path, "wb");
        if (output != NULL) {
            if (format == TABOS_CAMERA_FORMAT_RAW8) {
                fprintf(output, "P5\n%lu %lu\n255\n", (unsigned long) frame.width, (unsigned long) frame.height);
            }
            const int write_status = write_frame(stream, &frame, output);
            const int write_error  = errno;
            const int close_status = fclose(output);
            if (write_status == 0 && close_status == 0) {
                printf("wrote %lu bytes to %s; sequence=%lu dropped=%lu\n", (unsigned long) frame.size_bytes, path,
                       (unsigned long) frame.sequence, (unsigned long) frame.dropped_frames);
                status = 0;
            } else {
                if (write_status != 0) {
                    errno = write_error;
                }
                fprintf(stderr, "cameratest: write failed: %s\n", strerror(errno));
            }
        } else {
            fprintf(stderr, "cameratest: cannot open %s: %s\n", path, strerror(errno));
        }
        (void) tabos_camera_release(stream, frame.lease);
    } else {
        fprintf(stderr, "cameratest: capture failed: %s\n", strerror(errno));
    }
    (void) tabos_camera_close(stream);
    return status;
}

static int frame_count(const char* text, unsigned int fallback, unsigned int* count)
{
    if (text == NULL) {
        *count = fallback;
        return 0;
    }
    char* end                 = NULL;
    const unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0U || value > 10000U) {
        return -1;
    }
    *count = (unsigned int) value;
    return 0;
}

static int record_h264(const char* path, unsigned int count)
{
    const tabos_camera_stream_t stream = camera_open(TABOS_CAMERA_FORMAT_H264, NULL);
    if (stream == TABOS_CAMERA_STREAM_INVALID) {
        return 1;
    }
    FILE* output     = fopen(path, "wb");
    int status       = output == NULL ? 1 : 0;
    uint32_t dropped = 0U;
    for (unsigned int index = 0U; status == 0 && index < count; ++index) {
        tabos_camera_frame_t frame = {0};
        if (acquire(stream, &frame) != 0) {
            fprintf(stderr, "cameratest: H.264 frame %u acquire failed: %s\n", index + 1U, strerror(errno));
            status = 1;
        } else if (write_frame(stream, &frame, output) != 0) {
            fprintf(stderr, "cameratest: H.264 frame %u write failed: %s\n", index + 1U, strerror(errno));
            status = 1;
        } else {
            dropped = frame.dropped_frames;
        }
        if (frame.lease != TABOS_CAMERA_LEASE_INVALID) {
            if (tabos_camera_release(stream, frame.lease) != 0) {
                fprintf(stderr, "cameratest: H.264 frame %u release failed: %s\n", index + 1U, strerror(errno));
                status = 1;
            }
        }
    }
    if (output == NULL) {
        fprintf(stderr, "cameratest: cannot open %s: %s\n", path, strerror(errno));
    } else if (fclose(output) != 0) {
        fprintf(stderr, "cameratest: cannot close %s: %s\n", path, strerror(errno));
        status = 1;
    }
    if (tabos_camera_close(stream) != 0) {
        fprintf(stderr, "cameratest: close failed: %s\n", strerror(errno));
        status = 1;
    }
    if (status == 0) {
        printf("wrote %u H.264 frames to %s; dropped=%lu\n", count, path, (unsigned long) dropped);
    }
    return status;
}

static int preview(unsigned int count, const char* snapshot_path)
{
    if (snapshot_path != NULL) {
        puts("cameratest: preview snapshot: S saves displayed RGB565 frame; Q/Escape cancels");
    }
    tabos_camera_config_t config;
    const tabos_camera_stream_t stream = camera_open(TABOS_CAMERA_FORMAT_RGB565, &config);
    if (stream == TABOS_CAMERA_STREAM_INVALID) {
        return 1;
    }
    tabos_graphics_t graphics = {.width = config.width, .height = config.height};
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "cameratest: graphics open failed: %s\n", strerror(errno));
        (void) tabos_camera_close(stream);
        return 1;
    }
    int status = 0;
    bool saved = false;
    for (unsigned int index = 0U; index < count; ++index) {
        tabos_camera_frame_t frame = {0};
        tabos_input_event_t event;
        bool save = false;
        bool quit = false;
        while (tabos_input_poll(&event) > 0) {
            if (event.type == TABOS_INPUT_KEY_DOWN && (event.key == TABOS_KEY_Q || event.key == TABOS_KEY_ESCAPE)) {
                quit = true;
            }
            if (snapshot_path != NULL && event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_S) {
                save = true;
            }
        }
        if (quit) {
            break;
        }
        tabos_color_t* pixels   = tabos_graphics_pixels(&graphics);
        const uint32_t capacity = graphics.width * graphics.height * sizeof(*pixels);
        if (acquire(stream, &frame) != 0 || frame.width != graphics.width || frame.height != graphics.height ||
            tabos_camera_copy(stream, frame.lease, 0U, pixels, capacity) != (int) capacity ||
            tabos_graphics_present(&graphics) != 0) {
            status = 1;
        }
        if (status == 0 && save) {
            // Keep this exact displayed frame leased throughout the chunked SD write.
            FILE* output = fopen(snapshot_path, "wb");
            if (output == NULL) {
                status = 1;
            } else {
                const int write_status = write_frame(stream, &frame, output);
                const int write_error  = errno;
                const int close_status = fclose(output);
                if (write_status != 0 || close_status != 0) {
                    if (write_status != 0) {
                        errno = write_error;
                    }
                    status = 1;
                } else {
                    saved = true;
                }
            }
        }
        if (frame.lease != TABOS_CAMERA_LEASE_INVALID) {
            (void) tabos_camera_release(stream, frame.lease);
        }
        if (status != 0) {
            fprintf(stderr, "cameratest: preview frame failed: %s\n", strerror(errno));
            break;
        }
        if (saved) {
            break;
        }
    }
    (void) tabos_graphics_close(&graphics);
    (void) tabos_camera_close(stream);
    if (saved) {
        printf("saved displayed RGB565 frame to %s\n", snapshot_path);
    } else if (snapshot_path != NULL && status == 0) {
        puts("cameratest: snapshot cancelled; no file written");
    }
    return status;
}

static int parse_format(const char* name, tabos_camera_format_t* format)
{
    if (strcmp(name, "raw") == 0) {
        *format = TABOS_CAMERA_FORMAT_RAW8;
    } else if (strcmp(name, "rgb") == 0) {
        *format = TABOS_CAMERA_FORMAT_RGB565;
    } else if (strcmp(name, "jpeg") == 0) {
        *format = TABOS_CAMERA_FORMAT_JPEG;
    } else {
        return -1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "info") == 0)) {
        return show_info();
    }
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "capture") == 0) {
        tabos_camera_format_t format;
        if (parse_format(argv[2], &format) != 0) {
            fprintf(stderr, "cameratest: unknown capture format: %s\n", argv[2]);
            return 1;
        }
        return capture(format, argv[3]);
    }
    if ((argc == 2 || argc == 3) && strcmp(argv[1], "preview") == 0) {
        unsigned int count;
        return frame_count(argc == 3 ? argv[2] : NULL, 300U, &count) == 0 ? preview(count, NULL) : 1;
    }
    if (argc == 3 && strcmp(argv[1], "snapshot") == 0) {
        return preview(UINT32_MAX, argv[2]);
    }
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "record") == 0 && strcmp(argv[2], "h264") == 0) {
        unsigned int count;
        return frame_count(argc == 5 ? argv[4] : NULL, 100U, &count) == 0 ? record_h264(argv[3], count) : 1;
    }
    usage(stderr);
    return 1;
}
