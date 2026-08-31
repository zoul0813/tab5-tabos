#include <tabos/audio.h>
#include <tabos/platform/platform.h>

#include "bsp/m5stack_tab5.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <errno.h>
#include <stdatomic.h>

enum {
    TAB5_AUDIO_FRAMES     = 480,
    TAB5_AUDIO_TASK_STACK = 8192,
    TAB5_AUDIO_TASK_PRIO  = 5,
};

static esp_codec_dev_handle_t speaker_codec;
static esp_codec_dev_handle_t microphone_codec;
static TaskHandle_t audio_task;
static platform_audio_render_fn render_callback;
static platform_audio_capture_fn capture_callback;
static platform_audio_error_fn error_callback;
static atomic_bool audio_running;
static atomic_bool audio_task_active;

static void audio_worker(void* argument)
{
    (void) argument;
    int16_t playback[TAB5_AUDIO_FRAMES * 2U];
    int16_t capture[TAB5_AUDIO_FRAMES * 2U];
    while (atomic_load_explicit(&audio_running, memory_order_acquire)) {
        render_callback(playback, TAB5_AUDIO_FRAMES);
        if (esp_codec_dev_write(speaker_codec, playback, sizeof(playback)) != ESP_CODEC_DEV_OK ||
            esp_codec_dev_read(microphone_codec, capture, sizeof(capture)) != ESP_CODEC_DEV_OK) {
            error_callback(EIO);
            break;
        }
        capture_callback(capture, TAB5_AUDIO_FRAMES, 2U);
    }
    atomic_store_explicit(&audio_task_active, false, memory_order_release);
    vTaskDelete(NULL);
}

bool platform_audio_init(platform_audio_render_fn render, platform_audio_capture_fn capture,
                         platform_audio_error_fn error, platform_audio_info_t* info)
{
    if (render == NULL || capture == NULL || error == NULL || info == NULL) {
        return false;
    }
    *info = (platform_audio_info_t) {
        .driver           = "ES8388/ES7210",
        .features         = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
        .routes           = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE | TABOS_AUDIO_ROUTE_MICROPHONE,
        .capture_channels = 2U,
        .detected         = true,
        .ready            = false,
        .error            = EIO,
    };
    speaker_codec    = bsp_audio_codec_speaker_init();
    microphone_codec = bsp_audio_codec_microphone_init();
    if (speaker_codec == NULL || microphone_codec == NULL) {
        platform_audio_shutdown();
        return false;
    }
    esp_codec_dev_sample_info_t format = {
        .bits_per_sample = TABOS_AUDIO_SAMPLE_BITS,
        .channel         = 2U,
        .channel_mask    = 0U,
        .sample_rate     = TABOS_AUDIO_SAMPLE_RATE,
    };
    if (esp_codec_dev_open(speaker_codec, &format) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_open(microphone_codec, &format) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_out_vol(speaker_codec, 70) != ESP_CODEC_DEV_OK) {
        platform_audio_shutdown();
        return false;
    }
    render_callback  = render;
    capture_callback = capture;
    error_callback   = error;
    atomic_store_explicit(&audio_running, true, memory_order_release);
    atomic_store_explicit(&audio_task_active, true, memory_order_release);
    if (xTaskCreate(audio_worker, "tabos_audio", TAB5_AUDIO_TASK_STACK, NULL, TAB5_AUDIO_TASK_PRIO, &audio_task) !=
        pdPASS) {
        atomic_store_explicit(&audio_running, false, memory_order_release);
        atomic_store_explicit(&audio_task_active, false, memory_order_release);
        platform_audio_shutdown();
        return false;
    }
    info->ready = true;
    info->error = 0;
    return true;
}

void platform_audio_shutdown(void)
{
    atomic_store_explicit(&audio_running, false, memory_order_release);
    while (atomic_load_explicit(&audio_task_active, memory_order_acquire)) {
        vTaskDelay(1);
    }
    if (microphone_codec != NULL) {
        (void) esp_codec_dev_close(microphone_codec);
        esp_codec_dev_delete(microphone_codec);
        microphone_codec = NULL;
    }
    if (speaker_codec != NULL) {
        (void) esp_codec_dev_close(speaker_codec);
        esp_codec_dev_delete(speaker_codec);
        speaker_codec = NULL;
    }
    (void) bsp_feature_enable(BSP_FEATURE_SPEAKER, false);
    render_callback  = NULL;
    capture_callback = NULL;
    error_callback   = NULL;
}

bool platform_audio_set_route(uint32_t route)
{
    if (route == TABOS_AUDIO_ROUTE_SPEAKER) {
        return bsp_feature_enable(BSP_FEATURE_SPEAKER, true) == ESP_OK;
    }
    if (route == TABOS_AUDIO_ROUTE_HEADPHONE) {
        return bsp_feature_enable(BSP_FEATURE_SPEAKER, false) == ESP_OK;
    }
    return route == TABOS_AUDIO_ROUTE_MICROPHONE;
}
