#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

typedef int esp_err_t;
typedef int esp_h264_err_t;
typedef void* isp_proc_handle_t;
typedef void* jpeg_encoder_handle_t;
typedef void* esp_h264_enc_handle_t;
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef uint32_t TickType_t;

#define ESP_OK                       0
#define ESP_ERR_NO_MEM               12
#define ESP_H264_ERR_OK              0
#define BSP_CAMERA_DEVICE            "/camera"
#define BSP_FEATURE_CAMERA           1
#define MALLOC_CAP_INTERNAL          1
#define MALLOC_CAP_8BIT              2
#define MALLOC_CAP_SPIRAM            4
#define pdTRUE                       1
#define pdFALSE                      0
#define pdPASS                       1
#define portMAX_DELAY                UINT32_MAX
#define pdMS_TO_TICKS(ms)            ((TickType_t) (ms))
#define JPEG_DEC_ALLOC_OUTPUT_BUFFER 1
#define JPEG_ENCODE_IN_FORMAT_RGB565 1
#define JPEG_DOWN_SAMPLING_YUV422    1
#define ESP_H264_RAW_FMT_O_UYY_E_VYY 1
#define ESP_H264_MEM_SPIRAM          1

enum {
    V4L2_BUF_TYPE_VIDEO_CAPTURE = 1,
    V4L2_MEMORY_MMAP,
    V4L2_PIX_FMT_RGB565,
    V4L2_PIX_FMT_YUV420,
    V4L2_CAP_TIMEPERFRAME,
    V4L2_BUF_FLAG_ERROR = 8,
    V4L2_BUF_FLAG_DONE  = 16,
    VIDIOC_S_FMT        = 100,
    VIDIOC_G_FMT,
    VIDIOC_S_PARM,
    VIDIOC_S_DQBUF_TIMEOUT,
    VIDIOC_REQBUFS,
    VIDIOC_QUERYBUF,
    VIDIOC_QBUF,
    VIDIOC_DQBUF,
    VIDIOC_STREAMON,
    VIDIOC_STREAMOFF,
};

struct v4l2_pix_format {
        uint32_t width;
        uint32_t height;
        uint32_t pixelformat;
        uint32_t bytesperline;
        uint32_t sizeimage;
};
struct v4l2_format {
        uint32_t type;
        struct {
                struct v4l2_pix_format pix;
        } fmt;
};
struct v4l2_streamparm {
        uint32_t type;
        struct {
                struct {
                        uint32_t capability;
                        struct {
                                uint32_t numerator;
                                uint32_t denominator;
                        } timeperframe;
                } capture;
        } parm;
};
struct v4l2_requestbuffers {
        uint32_t count;
        uint32_t type;
        uint32_t memory;
};
struct v4l2_buffer {
        uint32_t type;
        uint32_t memory;
        uint32_t index;
        uint32_t length;
        uint32_t bytesused;
        uint32_t flags;
        struct {
                uint32_t offset;
        } m;
};

typedef struct {
        float matrix[3][3];
} esp_isp_ccm_config_t;
typedef struct {
        int timeout_ms;
} jpeg_encode_engine_cfg_t;
typedef struct {
        int buffer_direction;
} jpeg_encode_memory_alloc_cfg_t;
typedef struct {
        int src_type;
        int sub_sample;
        int image_quality;
        uint32_t width;
        uint32_t height;
} jpeg_encode_cfg_t;
typedef struct {
        int pic_type;
        uint32_t gop;
        uint32_t fps;
        struct {
                uint32_t width;
                uint32_t height;
        } res;
        struct {
                uint32_t bitrate;
                int qp_min;
                int qp_max;
        } rc;
} esp_h264_enc_cfg_hw_t;
typedef struct {
        uint8_t* buffer;
        uint32_t len;
} fake_h264_data_t;
typedef struct {
        fake_h264_data_t raw_data;
        uint64_t pts;
} esp_h264_enc_in_frame_t;
typedef struct {
        fake_h264_data_t raw_data;
        uint32_t length;
} esp_h264_enc_out_frame_t;

void fake_camera_log(const char* tag, const char* format, ...);
#define ESP_LOGE(tag, ...) fake_camera_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) fake_camera_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) fake_camera_log(tag, __VA_ARGS__)

int fake_camera_open(const char* path, int flags, ...);
int fake_camera_close(int descriptor);
int fake_camera_ioctl(int descriptor, unsigned long request, ...);
void* fake_camera_mmap(void* address, size_t size, int prot, int flags, int descriptor, off_t offset);
int fake_camera_munmap(void* address, size_t size);
#define open   fake_camera_open
#define close  fake_camera_close
#define ioctl  fake_camera_ioctl
#define mmap   fake_camera_mmap
#define munmap fake_camera_munmap

esp_err_t jpeg_encoder_process(jpeg_encoder_handle_t engine, const jpeg_encode_cfg_t* config, const uint8_t* input,
                               uint32_t input_size, uint8_t* output, uint32_t output_size, uint32_t* encoded_size);
esp_h264_err_t esp_h264_enc_process(esp_h264_enc_handle_t encoder, esp_h264_enc_in_frame_t* input,
                                    esp_h264_enc_out_frame_t* output);

static inline esp_err_t jpeg_new_encoder_engine(const jpeg_encode_engine_cfg_t* config, jpeg_encoder_handle_t* handle)
{
    (void) config;
    *handle = (void*) 1;
    return ESP_OK;
}
static inline uint8_t* jpeg_alloc_encoder_mem(size_t size, const jpeg_encode_memory_alloc_cfg_t* config, size_t* actual)
{
    (void) config;
    *actual = size;
    return calloc(1U, size);
}
static inline esp_err_t jpeg_del_encoder_engine(jpeg_encoder_handle_t handle)
{
    (void) handle;
    return ESP_OK;
}
static inline esp_h264_err_t esp_h264_enc_hw_new(const esp_h264_enc_cfg_hw_t* config, esp_h264_enc_handle_t* handle)
{
    (void) config;
    *handle = (void*) 1;
    return ESP_H264_ERR_OK;
}
static inline esp_h264_err_t esp_h264_enc_open(esp_h264_enc_handle_t handle)
{
    (void) handle;
    return ESP_H264_ERR_OK;
}
static inline esp_h264_err_t esp_h264_enc_close(esp_h264_enc_handle_t handle)
{
    (void) handle;
    return ESP_H264_ERR_OK;
}
static inline esp_h264_err_t esp_h264_enc_del(esp_h264_enc_handle_t handle)
{
    (void) handle;
    return ESP_H264_ERR_OK;
}
static inline uint8_t* esp_h264_aligned_calloc(size_t alignment, size_t count, uint32_t size, uint32_t* actual,
                                               int caps)
{
    (void) alignment;
    (void) caps;
    *actual = size;
    return calloc(count, size);
}
static inline void esp_h264_free(void* buffer)
{
    free(buffer);
}
static inline esp_err_t bsp_camera_start(void* config)
{
    (void) config;
    return ESP_OK;
}
static inline esp_err_t bsp_feature_enable(int feature, bool enable)
{
    (void) feature;
    (void) enable;
    return ESP_OK;
}
static inline esp_err_t esp_video_deinit(void)
{
    return ESP_OK;
}
static inline int64_t esp_timer_get_time(void)
{
    return 1000;
}
static inline size_t heap_caps_get_free_size(int caps)
{
    (void) caps;
    return 1000000U;
}
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    return (void*) 1;
}
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t ticks)
{
    (void) handle;
    return ticks == 0U ? pdFALSE : pdTRUE;
}
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t handle)
{
    (void) handle;
    return pdTRUE;
}
static inline void vSemaphoreDelete(SemaphoreHandle_t handle)
{
    (void) handle;
}
static inline void vTaskDelete(TaskHandle_t handle)
{
    (void) handle;
}
static inline void xTaskNotifyGive(TaskHandle_t handle)
{
    (void) handle;
}
static inline uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks)
{
    (void) clear;
    (void) ticks;
    return 1U;
}
static inline BaseType_t xTaskCreatePinnedToCore(void (*entry)(void*), const char* name, uint32_t stack, void* argument,
                                                 uint32_t priority, TaskHandle_t* task, int core)
{
    (void) entry;
    (void) name;
    (void) stack;
    (void) argument;
    (void) priority;
    (void) core;
    *task = (void*) 1;
    return pdPASS;
}
