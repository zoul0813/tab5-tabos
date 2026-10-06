#include <assert.h>
#include <stdarg.h>
#include <string.h>

// Exercise the production backend's private start/dequeue/stop paths, replacing
// only native hardware calls. No device or DMA worker runs in this fixture.
#include "../../platform/esp32p4/camera.c"

static struct v4l2_format retained_format;
static uint32_t reported_stride;
static uint32_t reported_image_size;
static bool wrong_dimensions;
static bool override_lengths;
static uint32_t mapping_lengths[2];
static uint32_t reported_bytes;
static uint32_t reported_index;
static uint32_t reported_flags;
static uint32_t encoded_bytes;
static unsigned int mappings;
static unsigned int unmaps;
static unsigned int live_mappings;
static unsigned int closes;
static unsigned int requeues;
static unsigned int submitted;
static unsigned int errors;
static unsigned int encoded;
static uint32_t encoder_input_bytes;
static size_t submitted_bytes;
static uint8_t copied_pixels[16];

void fake_camera_log(const char* tag, const char* format, ...)
{
    (void) tag;
    (void) format;
}

int fake_camera_open(const char* path, int flags, ...)
{
    assert(strcmp(path, BSP_CAMERA_DEVICE) == 0 && flags == O_RDONLY);
    return 9;
}

int fake_camera_close(int descriptor)
{
    assert(descriptor == 9);
    ++closes;
    return 0;
}

int fake_camera_ioctl(int descriptor, unsigned long request, ...)
{
    assert(descriptor == 9);
    va_list arguments;
    va_start(arguments, request);
    void* argument = va_arg(arguments, void*);
    va_end(arguments);
    if (request == VIDIOC_S_FMT) {
        retained_format = *(struct v4l2_format*) argument;
    } else if (request == VIDIOC_G_FMT) {
        struct v4l2_format* format   = argument;
        *format                      = retained_format;
        format->fmt.pix.bytesperline = reported_stride;
        format->fmt.pix.sizeimage    = reported_image_size;
        if (wrong_dimensions) {
            ++format->fmt.pix.width;
        }
    } else if (request == VIDIOC_QUERYBUF) {
        struct v4l2_buffer* buffer = argument;
        buffer->length             = capture_frame_bytes;
        if (override_lengths) {
            buffer->length = mapping_lengths[buffer->index];
        }
    } else if (request == VIDIOC_DQBUF) {
        struct v4l2_buffer* buffer = argument;
        buffer->index              = reported_index;
        buffer->flags              = reported_flags;
        buffer->bytesused          = reported_bytes;
    } else if (request == VIDIOC_QBUF) {
        ++requeues;
    } else {
        assert(request == VIDIOC_S_PARM || request == VIDIOC_S_DQBUF_TIMEOUT || request == VIDIOC_REQBUFS ||
               request == VIDIOC_STREAMON || request == VIDIOC_STREAMOFF);
    }
    return 0;
}

void* fake_camera_mmap(void* address, size_t size, int prot, int flags, int descriptor, off_t offset)
{
    (void) address;
    (void) offset;
    assert(prot == (PROT_READ | PROT_WRITE) && flags == MAP_SHARED && descriptor == 9);
    uint8_t* data = malloc(size);
    assert(data != NULL);
    memset(data, 0xaa, size);
    const uint16_t colors[] = {0xf800U, 0x07e0U, 0x001fU, 0xffffU};
    if (size >= sizeof(colors)) {
        memcpy(data, colors, sizeof(colors));
    }
    ++mappings;
    ++live_mappings;
    return data;
}

int fake_camera_munmap(void* address, size_t size)
{
    assert(address != NULL && size != 0U && live_mappings > 0U);
    free(address);
    ++unmaps;
    --live_mappings;
    return 0;
}

esp_err_t jpeg_encoder_process(jpeg_encoder_handle_t engine, const jpeg_encode_cfg_t* config, const uint8_t* input,
                               uint32_t input_size, uint8_t* output, uint32_t output_size, uint32_t* encoded_size)
{
    assert(engine != NULL && config != NULL && input != NULL && output != NULL && output_size != 0U);
    ++encoded;
    encoder_input_bytes = input_size;
    *encoded_size       = encoded_bytes;
    return ESP_OK;
}

esp_h264_err_t esp_h264_enc_process(esp_h264_enc_handle_t encoder, esp_h264_enc_in_frame_t* input,
                                    esp_h264_enc_out_frame_t* output)
{
    assert(encoder != NULL && input->raw_data.buffer != NULL && output->raw_data.buffer != NULL);
    ++encoded;
    encoder_input_bytes = input->raw_data.len;
    output->length      = encoded_bytes;
    return ESP_H264_ERR_OK;
}

esp_err_t __real_esp_isp_ccm_configure(isp_proc_handle_t handle, const esp_isp_ccm_config_t* config)
{
    (void) handle;
    (void) config;
    return ESP_OK;
}

uint64_t platform_time_ms(void)
{
    return 1U;
}

static void submit(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t stride, uint32_t format,
                   uint64_t timestamp)
{
    (void) stride;
    (void) format;
    (void) timestamp;
    assert(width == 2U && height == 2U && size <= sizeof(copied_pixels));
    memcpy(copied_pixels, data, size);
    submitted_bytes = size;
    ++submitted;
}

static void failure(int error)
{
    assert(error == EIO);
    ++errors;
}

static tabos_camera_config_t reset(uint32_t format)
{
    assert(live_mappings == 0U && camera_fd == -1 && !streaming && capture_frame_bytes == 0U);
    initialized         = true;
    submit_frame        = submit;
    submit_error        = failure;
    capture_idle        = (void*) 1;
    capture_task        = (void*) 1;
    reported_stride     = 0U;
    reported_image_size = 0U;
    wrong_dimensions    = false;
    override_lengths    = false;
    reported_flags      = V4L2_BUF_FLAG_DONE;
    reported_index      = 0U;
    encoded_bytes       = 3U;
    mappings            = 0U;
    unmaps              = 0U;
    closes              = 0U;
    submitted           = 0U;
    errors              = 0U;
    encoded             = 0U;
    return (tabos_camera_config_t) {.format = format, .width = 2U, .height = 2U, .fps = 30U};
}

static void start(const tabos_camera_config_t* config)
{
    assert(platform_camera_start(config));
    assert(mappings == 2U && live_mappings == 2U);
    reported_bytes = capture_frame_bytes;
    requeues       = 0U;
}

static void stop(void)
{
    platform_camera_stop();
    assert(live_mappings == 0U && unmaps == mappings && closes == 1U && capture_frame_bytes == 0U);
    assert(jpeg_output == NULL && h264_output == NULL && camera_fd == -1 && !streaming);
}

static void test_layout_and_partial_start(void)
{
    tabos_camera_config_t config = reset(TABOS_CAMERA_FORMAT_RAW8);
    reported_stride              = 6U;
    assert(!platform_camera_start(&config));
    assert(mappings == 0U && closes == 1U);
    config              = reset(TABOS_CAMERA_FORMAT_RAW8);
    reported_image_size = 4U;
    assert(!platform_camera_start(&config));
    assert(mappings == 0U && closes == 1U);
    config           = reset(TABOS_CAMERA_FORMAT_RAW8);
    wrong_dimensions = true;
    assert(!platform_camera_start(&config));
    assert(mappings == 0U && closes == 1U);
    for (unsigned int index = 0U; index < 2U; ++index) {
        config                 = reset(TABOS_CAMERA_FORMAT_RAW8);
        override_lengths       = true;
        mapping_lengths[0]     = 8U;
        mapping_lengths[1]     = 8U;
        mapping_lengths[index] = 2U;
        assert(!platform_camera_start(&config));
        assert(mappings == index && unmaps == index && live_mappings == 0U && closes == 1U);
    }
    config              = reset(TABOS_CAMERA_FORMAT_RAW8);
    reported_image_size = 16U;
    assert(!platform_camera_start(&config));
    assert(mappings == 0U && closes == 1U);
}

static void test_completed_payloads(void)
{
    for (uint32_t format = TABOS_CAMERA_FORMAT_RAW8; format < TABOS_CAMERA_FORMAT_COUNT; ++format) {
        for (unsigned int scenario = 0U; scenario < 4U; ++scenario) {
            const tabos_camera_config_t config = reset(format);
            start(&config);
            if (scenario == 0U) {
                reported_bytes = capture_frame_bytes - 1U;
            } else if (scenario == 1U) {
                reported_bytes = capture_frame_bytes + 1U;
            } else if (scenario == 2U) {
                free(buffers[0].data);
                buffers[0].data = calloc(1U, 2U);
                buffers[0].size = 2U;
            } else {
                reported_index = CAMERA_BUFFER_COUNT;
            }
            assert(!capture_one_frame());
            assert(errors == 1U && submitted == 0U && encoded == 0U && requeues == 0U);
            stop();
        }
    }
}

static void test_valid_frames_and_encoder_bounds(void)
{
    for (uint32_t format = TABOS_CAMERA_FORMAT_RAW8; format < TABOS_CAMERA_FORMAT_COUNT; ++format) {
        tabos_camera_config_t config = reset(format);
        start(&config);
        assert(capture_one_frame());
        assert(submitted == 1U && errors == 0U && requeues == 1U);
        if (format == TABOS_CAMERA_FORMAT_RAW8) {
            const uint8_t gray[] = {76U, 149U, 28U, 255U};
            assert(submitted_bytes == sizeof(gray) && memcmp(copied_pixels, gray, sizeof(gray)) == 0);
        } else if (format == TABOS_CAMERA_FORMAT_RGB565) {
            assert(submitted_bytes == 8U);
        } else {
            assert(submitted_bytes == 3U && encoded == 1U && encoder_input_bytes == capture_frame_bytes);
        }
        stop();
        if (format == TABOS_CAMERA_FORMAT_JPEG || format == TABOS_CAMERA_FORMAT_H264) {
            for (unsigned int scenario = 0U; scenario < 2U; ++scenario) {
                config = reset(format);
                start(&config);
                encoded_bytes = scenario == 0U ? 0U : capture_frame_bytes + 1U;
                assert(!capture_one_frame());
                assert(encoded == 1U && errors == 1U && submitted == 0U && requeues == 0U);
                stop();
            }
        }
    }
    const tabos_camera_config_t config = reset(TABOS_CAMERA_FORMAT_RGB565);
    reported_stride                    = 4U;
    reported_image_size                = 16U;
    override_lengths                   = true;
    mapping_lengths[0]                 = 16U;
    mapping_lengths[1]                 = 16U;
    start(&config);
    reported_bytes = 16U;
    assert(capture_one_frame() && submitted_bytes == 8U && errors == 0U);
    stop();
}

static void test_error_flag_preserves_warmup_skip(void)
{
    const tabos_camera_config_t config = reset(TABOS_CAMERA_FORMAT_RAW8);
    start(&config);
    reported_flags = V4L2_BUF_FLAG_ERROR;
    reported_bytes = 0U;
    assert(capture_one_frame());
    assert(errors == 0U && submitted == 0U && encoded == 0U && requeues == 1U);
    stop();
}

int main(void)
{
    test_layout_and_partial_start();
    test_completed_payloads();
    test_valid_frames_and_encoder_bounds();
    test_error_flag_preserves_warmup_skip();
    return 0;
}
