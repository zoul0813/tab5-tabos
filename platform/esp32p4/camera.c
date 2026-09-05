#include <tabos/platform/platform.h>
#include "camera_ccm.h"

#include <bsp/m5stack_tab5.h>
#include <driver/jpeg_encode.h>
#include <driver/isp_ccm.h>
#include <esp_h264_alloc.h>
#include <esp_h264_enc_single.h>
#include <esp_h264_enc_single_hw.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_video_device.h>
#include <esp_video_init.h>
#include <esp_video_ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

enum {
    CAMERA_BUFFER_COUNT = 2,
    CAMERA_JPEG_QUALITY = 80,
    CAMERA_ERROR_LIMIT  = 30
};

typedef struct {
        void* data;
        size_t size;
} camera_buffer_t;

static const char* TAG = "tabos-camera";
static camera_buffer_t buffers[CAMERA_BUFFER_COUNT];
static platform_camera_frame_fn submit_frame;
static platform_camera_error_fn submit_error;
static tabos_camera_config_t active_config;
static jpeg_encoder_handle_t jpeg_encoder;
static uint8_t* jpeg_output;
static size_t jpeg_output_size;
static esp_h264_enc_handle_t h264_encoder;
static uint8_t* h264_output;
static uint32_t h264_output_size;
static int camera_fd = -1;
static bool initialized;
static bool streaming;
static uint32_t consecutive_capture_errors;
static bool dequeue_warning_logged;
static uint64_t capture_started_us;
static uint64_t conversion_us;
static uint64_t encoding_us;
static uint32_t measured_frames;
static uint32_t dequeue_misses;
static uint64_t last_dequeue_us;

// Apply hardware bounds after esp_video folds white-balance gains into columns.
// Operate on a copy; vendor state and already-valid matrices remain unchanged.
esp_err_t __real_esp_isp_ccm_configure(isp_proc_handle_t proc, const esp_isp_ccm_config_t* config);
esp_err_t __wrap_esp_isp_ccm_configure(isp_proc_handle_t proc, const esp_isp_ccm_config_t* config)
{
    static atomic_flag logged           = ATOMIC_FLAG_INIT;
    static atomic_flag corrected_logged = ATOMIC_FLAG_INIT;
    esp_isp_ccm_config_t bounded;
    const esp_isp_ccm_config_t* applied = config;
    if (proc != NULL && config != NULL) {
        bounded = *config;
        if (esp32p4_camera_bound_ccm(bounded.matrix)) {
            applied = &bounded;
            if (!atomic_flag_test_and_set(&corrected_logged)) {
                ESP_LOGW(TAG, "CCM bounded after white balance; neutral response preserved");
            }
        }
    }
    const esp_err_t result = __real_esp_isp_ccm_configure(proc, applied);
    if (result != ESP_OK && config != NULL && !atomic_flag_test_and_set(&logged)) {
        ESP_LOGE(TAG, "CCM rejected: result=%d matrix=[%.6f %.6f %.6f; %.6f %.6f %.6f; %.6f %.6f %.6f]", result,
                 (double) config->matrix[0][0], (double) config->matrix[0][1], (double) config->matrix[0][2],
                 (double) config->matrix[1][0], (double) config->matrix[1][1], (double) config->matrix[1][2],
                 (double) config->matrix[2][0], (double) config->matrix[2][1], (double) config->matrix[2][2]);
    }
    return result;
}

static void report_error(int error)
{
    ESP_LOGE(TAG, "camera pipeline failed: %d", error);
    if (submit_error != NULL) {
        submit_error(error);
    }
}

static void release_buffers(void)
{
    for (size_t index = 0U; index < CAMERA_BUFFER_COUNT; ++index) {
        if (buffers[index].data != NULL) {
            (void) munmap(buffers[index].data, buffers[index].size);
            buffers[index] = (camera_buffer_t) {0};
        }
    }
    if (jpeg_output != NULL) {
        free(jpeg_output);
        jpeg_output      = NULL;
        jpeg_output_size = 0U;
    }
    if (jpeg_encoder != NULL) {
        (void) jpeg_del_encoder_engine(jpeg_encoder);
        jpeg_encoder = NULL;
    }
    if (h264_encoder != NULL) {
        (void) esp_h264_enc_close(h264_encoder);
        (void) esp_h264_enc_del(h264_encoder);
        h264_encoder = NULL;
    }
    if (h264_output != NULL) {
        esp_h264_free(h264_output);
        h264_output      = NULL;
        h264_output_size = 0U;
    }
}

static bool configure_jpeg(void)
{
    const jpeg_encode_engine_cfg_t engine_config = {.timeout_ms = 5000};
    if (jpeg_new_encoder_engine(&engine_config, &jpeg_encoder) != ESP_OK) {
        return false;
    }
    const size_t requested_size                        = (size_t) active_config.width * active_config.height * 2U;
    const jpeg_encode_memory_alloc_cfg_t memory_config = {.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    jpeg_output = jpeg_alloc_encoder_mem(requested_size, &memory_config, &jpeg_output_size);
    return jpeg_output != NULL;
}

static bool configure_h264(void)
{
    const esp_h264_enc_cfg_hw_t encoder_config = {
        .pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY,
        .gop      = active_config.fps,
        .fps      = active_config.fps,
        .res      = {.width = active_config.width, .height = active_config.height},
        .rc       = {.bitrate = active_config.width * active_config.height * active_config.fps / 8U,
                     .qp_min  = 10,
                     .qp_max  = 40},
    };
    if (esp_h264_enc_hw_new(&encoder_config, &h264_encoder) != ESP_H264_ERR_OK ||
        esp_h264_enc_open(h264_encoder) != ESP_H264_ERR_OK) {
        return false;
    }
    h264_output_size        = active_config.width * active_config.height * 3U / 2U;
    uint32_t allocated_size = 0U;
    h264_output             = esp_h264_aligned_calloc(16U, 1U, h264_output_size, &allocated_size, ESP_H264_MEM_SPIRAM);
    h264_output_size        = allocated_size;
    return h264_output != NULL;
}

static bool configure_capture(void)
{
    camera_fd = open(BSP_CAMERA_DEVICE, O_RDONLY);
    if (camera_fd < 0) {
        ESP_LOGE(TAG, "failed to open %s: errno=%d", BSP_CAMERA_DEVICE, errno);
        return false;
    }
    uint32_t pixel_format = V4L2_PIX_FMT_RGB565;
    if (active_config.format == TABOS_CAMERA_FORMAT_H264) {
        pixel_format = V4L2_PIX_FMT_YUV420;
    }
    struct v4l2_format format  = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    format.fmt.pix.width       = active_config.width;
    format.fmt.pix.height      = active_config.height;
    format.fmt.pix.pixelformat = pixel_format;
    if (ioctl(camera_fd, VIDIOC_S_FMT, &format) != 0 || format.fmt.pix.width != active_config.width ||
        format.fmt.pix.height != active_config.height || format.fmt.pix.pixelformat != pixel_format) {
        ESP_LOGE(TAG, "unsupported capture format %lux%lu fourcc=0x%08lx: errno=%d",
                 (unsigned long) active_config.width, (unsigned long) active_config.height,
                 (unsigned long) pixel_format, errno);
        return false;
    }
    struct v4l2_streamparm parameters                = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    parameters.parm.capture.capability               = V4L2_CAP_TIMEPERFRAME;
    parameters.parm.capture.timeperframe.numerator   = 1U;
    parameters.parm.capture.timeperframe.denominator = active_config.fps;
    if (ioctl(camera_fd, VIDIOC_S_PARM, &parameters) != 0) {
        ESP_LOGE(TAG, "unsupported capture rate %lu fps: errno=%d", (unsigned long) active_config.fps, errno);
        return false;
    }
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 10000};
    if (ioctl(camera_fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        ESP_LOGE(TAG, "failed to set dequeue timeout: errno=%d", errno);
        return false;
    }
    struct v4l2_requestbuffers request = {
        .count = CAMERA_BUFFER_COUNT, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
    if (ioctl(camera_fd, VIDIOC_REQBUFS, &request) != 0 || request.count < CAMERA_BUFFER_COUNT) {
        ESP_LOGE(TAG, "failed to allocate capture buffers: count=%lu errno=%d", (unsigned long) request.count, errno);
        return false;
    }
    for (size_t index = 0U; index < CAMERA_BUFFER_COUNT; ++index) {
        struct v4l2_buffer buffer = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP, .index = index};
        if (ioctl(camera_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "failed to query capture buffer %lu: errno=%d", (unsigned long) index, errno);
            return false;
        }
        buffers[index].data = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera_fd, buffer.m.offset);
        buffers[index].size = buffer.length;
        if (buffers[index].data == MAP_FAILED) {
            buffers[index].data = NULL;
            ESP_LOGE(TAG, "failed to map capture buffer %lu: errno=%d", (unsigned long) index, errno);
            return false;
        }
        if (ioctl(camera_fd, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "failed to queue capture buffer %lu: errno=%d", (unsigned long) index, errno);
            return false;
        }
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start capture stream: errno=%d", errno);
        return false;
    }
    return true;
}

bool platform_camera_init(platform_camera_frame_fn frame, platform_camera_error_fn error, platform_camera_info_t* info)
{
    if (frame == NULL || error == NULL || info == NULL) {
        return false;
    }
    submit_frame           = frame;
    submit_error           = error;
    const esp_err_t result = bsp_camera_start(NULL);
    initialized            = result == ESP_OK;
    *info                  = (platform_camera_info_t) {
                         .driver  = "SC2356 (SC202CS-compatible)",
                         .formats = TABOS_CAMERA_FORMAT_FLAG_RAW8 | TABOS_CAMERA_FORMAT_FLAG_RGB565 | TABOS_CAMERA_FORMAT_FLAG_JPEG |
                   TABOS_CAMERA_FORMAT_FLAG_H264,
                         .max_width  = 1600U,
                         .max_height = 1200U,
                         .max_fps    = 30U,
                         .detected   = initialized,
                         .ready      = initialized,
                         .error      = initialized ? 0 : result,
    };
    return initialized;
}

bool platform_camera_start(const tabos_camera_config_t* config)
{
    if (!initialized || streaming || config == NULL) {
        return false;
    }
    active_config = *config;
    if (config->format == TABOS_CAMERA_FORMAT_JPEG && !configure_jpeg()) {
        release_buffers();
        return false;
    } else if (config->format == TABOS_CAMERA_FORMAT_H264 && !configure_h264()) {
        release_buffers();
        return false;
    }
    if (!configure_capture()) {
        platform_camera_stop();
        return false;
    }
    streaming          = true;
    capture_started_us = (uint64_t) esp_timer_get_time();
    conversion_us      = 0U;
    encoding_us        = 0U;
    measured_frames    = 0U;
    dequeue_misses     = 0U;
    last_dequeue_us    = capture_started_us;
    ESP_LOGI(TAG, "capture start: format=%u %lux%lu@%lu internal_free=%lu psram_free=%lu",
             (unsigned int) config->format, (unsigned long) config->width, (unsigned long) config->height,
             (unsigned long) config->fps,
             (unsigned long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned long) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return true;
}

void platform_camera_stop(void)
{
    if (streaming) {
        const uint64_t elapsed_us = (uint64_t) esp_timer_get_time() - capture_started_us;
        ESP_LOGI(TAG, "capture stop: frames=%lu elapsed_us=%llu conversion_us=%llu encoding_us=%llu dequeue_misses=%lu",
                 (unsigned long) measured_frames, (unsigned long long) elapsed_us, (unsigned long long) conversion_us,
                 (unsigned long long) encoding_us, (unsigned long) dequeue_misses);
    }
    if (camera_fd >= 0) {
        if (streaming) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            (void) ioctl(camera_fd, VIDIOC_STREAMOFF, &type);
        }
        release_buffers();
        (void) close(camera_fd);
        camera_fd = -1;
    } else {
        release_buffers();
    }
    streaming                  = false;
    consecutive_capture_errors = 0U;
    dequeue_warning_logged     = false;
}

void platform_camera_update(void)
{
    if (!streaming) {
        return;
    }
    struct v4l2_buffer buffer = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
    if (ioctl(camera_fd, VIDIOC_DQBUF, &buffer) != 0) {
        const int error = errno;
        ++dequeue_misses;
        // Pinned esp_video returns ESP_FAIL for a timed-out ready semaphore;
        // its VFS maps that to EPERM. Do not warn on each ordinary 10 ms wait.
        // EPERM can also cover preprocessing errors, so retain a prolonged-stall warning.
        const bool empty_wait     = error == EAGAIN || error == ETIMEDOUT || error == EPERM;
        const uint64_t stalled_us = (uint64_t) esp_timer_get_time() - last_dequeue_us;
        if (!dequeue_warning_logged && (!empty_wait || stalled_us >= 2000000U)) {
            ESP_LOGW(TAG, "camera dequeue stalled: errno=%d elapsed_us=%llu", error, (unsigned long long) stalled_us);
            dequeue_warning_logged = true;
        }
        return;
    }
    dequeue_warning_logged = false;
    last_dequeue_us        = (uint64_t) esp_timer_get_time();
    if (buffer.index >= CAMERA_BUFFER_COUNT) {
        ESP_LOGE(TAG, "invalid dequeued buffer: index=%lu flags=0x%08lx bytes=%lu", (unsigned long) buffer.index,
                 (unsigned long) buffer.flags, (unsigned long) buffer.bytesused);
        report_error(EIO);
    } else if ((buffer.flags & V4L2_BUF_FLAG_ERROR) != 0U) {
        ++consecutive_capture_errors;
        if (consecutive_capture_errors == 1U) {
            ESP_LOGW(TAG, "skipping camera warm-up/error buffer: flags=0x%08lx bytes=%lu", (unsigned long) buffer.flags,
                     (unsigned long) buffer.bytesused);
        }
        if (consecutive_capture_errors >= CAMERA_ERROR_LIMIT) {
            ESP_LOGE(TAG, "camera returned %lu consecutive error buffers", (unsigned long) consecutive_capture_errors);
            report_error(EIO);
        }
    } else if ((buffer.flags & V4L2_BUF_FLAG_DONE) != 0U) {
        consecutive_capture_errors           = 0U;
        const uint64_t timestamp             = platform_time_ms();
        const uint64_t processing_started_us = (uint64_t) esp_timer_get_time();
        ++measured_frames;
        if (active_config.format == TABOS_CAMERA_FORMAT_RAW8) {
            const uint16_t* input = buffers[buffer.index].data;
            uint8_t* output       = buffers[buffer.index].data;
            const size_t pixels   = (size_t) active_config.width * active_config.height;
            for (size_t index = 0U; index < pixels; ++index) {
                const uint16_t pixel = input[index];
                const uint32_t red   = ((pixel >> 11U) & 0x1fU) * 255U / 31U;
                const uint32_t green = ((pixel >> 5U) & 0x3fU) * 255U / 63U;
                const uint32_t blue  = (pixel & 0x1fU) * 255U / 31U;
                output[index]        = (uint8_t) ((red * 77U + green * 150U + blue * 29U) >> 8U);
            }
            conversion_us += (uint64_t) esp_timer_get_time() - processing_started_us;
            submit_frame(output, pixels, active_config.width, active_config.height, active_config.width,
                         TABOS_CAMERA_FORMAT_RAW8, timestamp);
        } else if (active_config.format == TABOS_CAMERA_FORMAT_RGB565) {
            submit_frame(buffers[buffer.index].data, buffer.bytesused, active_config.width, active_config.height,
                         active_config.width * 2U, TABOS_CAMERA_FORMAT_RGB565, timestamp);
        } else if (active_config.format == TABOS_CAMERA_FORMAT_JPEG) {
            const jpeg_encode_cfg_t config = {.src_type      = JPEG_ENCODE_IN_FORMAT_RGB565,
                                              .sub_sample    = JPEG_DOWN_SAMPLING_YUV422,
                                              .image_quality = CAMERA_JPEG_QUALITY,
                                              .width         = active_config.width,
                                              .height        = active_config.height};
            uint32_t encoded_size          = 0U;
            const esp_err_t result =
                jpeg_encoder_process(jpeg_encoder, &config, buffers[buffer.index].data, buffer.bytesused, jpeg_output,
                                     jpeg_output_size, &encoded_size);
            encoding_us += (uint64_t) esp_timer_get_time() - processing_started_us;
            if (result == ESP_OK) {
                submit_frame(jpeg_output, encoded_size, active_config.width, active_config.height, 0U,
                             TABOS_CAMERA_FORMAT_JPEG, timestamp);
            } else {
                ESP_LOGE(TAG, "JPEG encode failed: result=%d input=%lu expected=%lu output_capacity=%lu", result,
                         (unsigned long) buffer.bytesused,
                         (unsigned long) active_config.width * active_config.height * 2UL,
                         (unsigned long) jpeg_output_size);
                report_error(result);
            }
        } else {
            esp_h264_enc_in_frame_t input = {
                .raw_data = {.buffer = buffers[buffer.index].data, .len = buffer.bytesused},
                  .pts = timestamp
            };
            esp_h264_enc_out_frame_t output = {
                .raw_data = {.buffer = h264_output, .len = h264_output_size}
            };
            const esp_h264_err_t result  = esp_h264_enc_process(h264_encoder, &input, &output);
            encoding_us                 += (uint64_t) esp_timer_get_time() - processing_started_us;
            if (result == ESP_H264_ERR_OK) {
                submit_frame(h264_output, output.length, active_config.width, active_config.height, 0U,
                             TABOS_CAMERA_FORMAT_H264, timestamp);
            } else {
                report_error(result);
            }
        }
    }
    if (ioctl(camera_fd, VIDIOC_QBUF, &buffer) != 0) {
        ESP_LOGE(TAG, "failed to requeue buffer %lu: flags=0x%08lx errno=%d", (unsigned long) buffer.index,
                 (unsigned long) buffer.flags, errno);
        report_error(EIO);
    }
}

void platform_camera_shutdown(void)
{
    platform_camera_stop();
    if (initialized) {
        (void) esp_video_deinit();
        (void) bsp_feature_enable(BSP_FEATURE_CAMERA, false);
        initialized = false;
    }
    submit_frame = NULL;
    submit_error = NULL;
}
