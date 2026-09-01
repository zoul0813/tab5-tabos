#include <tabos/audio.h>
#include <tabos/platform/platform.h>

#include "bsp/m5stack_tab5.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <errno.h>
#include <stdatomic.h>

enum {
    TAB5_AUDIO_MAX_FRAMES     = 480,
    TAB5_AUDIO_TASK_STACK     = 8192,
    TAB5_AUDIO_TASK_PRIO      = 5,
    TAB5_HEADPHONE_TASK_STACK = 3072,
    TAB5_HEADPHONE_TASK_PRIO  = 4,
    TAB5_HEADPHONE_POLL_MS    = 50,
    TAB5_HEADPHONE_DEBOUNCE   = 2,
    TAB5_HEADPHONE_DETECT_PIN = IO_EXPANDER_PIN_NUM_7,
};

static esp_codec_dev_handle_t speaker_codec;
static esp_codec_dev_handle_t microphone_codec;
static esp_io_expander_handle_t headphone_expander;
static TaskHandle_t audio_task;
static SemaphoreHandle_t route_mutex;
static platform_audio_render_fn render_callback;
static platform_audio_capture_fn capture_callback;
static platform_audio_error_fn error_callback;
static atomic_bool audio_running;
static atomic_bool audio_task_active;
static atomic_bool headphone_monitor_running;
static atomic_bool headphone_task_active;
static atomic_uint audio_sample_rate;
static bool speaker_route_requested;
static bool headphones_inserted;
static bool speaker_enabled;

static size_t chunk_frames(uint32_t sample_rate)
{
    size_t frames = sample_rate / 100U;
    if (frames > TAB5_AUDIO_MAX_FRAMES) {
        frames = TAB5_AUDIO_MAX_FRAMES;
    }
    return frames;
}

static void audio_worker(void* argument)
{
    (void) argument;
    int16_t playback[TAB5_AUDIO_MAX_FRAMES * 2U];
    int16_t capture[TAB5_AUDIO_MAX_FRAMES * 2U];
    while (atomic_load_explicit(&audio_running, memory_order_acquire)) {
        const size_t frames = chunk_frames(atomic_load_explicit(&audio_sample_rate, memory_order_acquire));
        const size_t bytes  = frames * 2U * sizeof(int16_t);
        render_callback(playback, frames);
        if (esp_codec_dev_write(speaker_codec, playback, bytes) != ESP_CODEC_DEV_OK ||
            esp_codec_dev_read(microphone_codec, capture, bytes) != ESP_CODEC_DEV_OK) {
            error_callback(EIO);
            break;
        }
        capture_callback(capture, frames, 2U);
    }
    atomic_store_explicit(&audio_task_active, false, memory_order_release);
    vTaskDelete(NULL);
}

static bool read_headphones_inserted(bool* inserted)
{
    uint32_t level = 0U;
    if (inserted == NULL || headphone_expander == NULL ||
        esp_io_expander_get_level(headphone_expander, TAB5_HEADPHONE_DETECT_PIN, &level) != ESP_OK) {
        return false;
    }
    *inserted = (level & TAB5_HEADPHONE_DETECT_PIN) != 0U;
    return true;
}

static bool apply_output_route_locked(void)
{
    const bool enable_speaker = speaker_route_requested && !headphones_inserted;
    if (enable_speaker == speaker_enabled) {
        return true;
    }
    if (bsp_feature_enable(BSP_FEATURE_SPEAKER, enable_speaker) != ESP_OK) {
        return false;
    }
    speaker_enabled = enable_speaker;
    return true;
}

static void headphone_worker(void* argument)
{
    (void) argument;
    bool candidate          = headphones_inserted;
    uint32_t stable_samples = 0U;
    while (atomic_load_explicit(&headphone_monitor_running, memory_order_acquire)) {
        bool inserted = false;
        if (read_headphones_inserted(&inserted)) {
            if (inserted == candidate) {
                ++stable_samples;
            } else {
                candidate      = inserted;
                stable_samples = 1U;
            }
            if (stable_samples >= TAB5_HEADPHONE_DEBOUNCE) {
                xSemaphoreTake(route_mutex, portMAX_DELAY);
                if (headphones_inserted != candidate) {
                    const bool previous = headphones_inserted;
                    headphones_inserted = candidate;
                    if (!apply_output_route_locked()) {
                        headphones_inserted = previous;
                    }
                }
                xSemaphoreGive(route_mutex);
                stable_samples = 0U;
            }
        } else {
            stable_samples = 0U;
        }
        vTaskDelay(pdMS_TO_TICKS(TAB5_HEADPHONE_POLL_MS));
    }
    atomic_store_explicit(&headphone_task_active, false, memory_order_release);
    vTaskDelete(NULL);
}

static bool start_headphone_monitor(void)
{
    atomic_store_explicit(&headphone_monitor_running, true, memory_order_release);
    atomic_store_explicit(&headphone_task_active, true, memory_order_release);
    if (xTaskCreate(headphone_worker, "tabos_headphones", TAB5_HEADPHONE_TASK_STACK, NULL, TAB5_HEADPHONE_TASK_PRIO,
                    NULL) == pdPASS) {
        return true;
    }
    atomic_store_explicit(&headphone_monitor_running, false, memory_order_release);
    atomic_store_explicit(&headphone_task_active, false, memory_order_release);
    return false;
}

static void stop_headphone_monitor(void)
{
    atomic_store_explicit(&headphone_monitor_running, false, memory_order_release);
    while (atomic_load_explicit(&headphone_task_active, memory_order_acquire)) {
        vTaskDelay(1);
    }
}

static void stop_audio_task(void)
{
    atomic_store_explicit(&audio_running, false, memory_order_release);
    while (atomic_load_explicit(&audio_task_active, memory_order_acquire)) {
        vTaskDelay(1);
    }
}

static bool start_audio_task(void)
{
    atomic_store_explicit(&audio_running, true, memory_order_release);
    atomic_store_explicit(&audio_task_active, true, memory_order_release);
    if (xTaskCreate(audio_worker, "tabos_audio", TAB5_AUDIO_TASK_STACK, NULL, TAB5_AUDIO_TASK_PRIO, &audio_task) ==
        pdPASS) {
        return true;
    }
    atomic_store_explicit(&audio_running, false, memory_order_release);
    atomic_store_explicit(&audio_task_active, false, memory_order_release);
    return false;
}

static bool configure_codecs(uint32_t sample_rate)
{
    esp_codec_dev_sample_info_t format = {
        .bits_per_sample = TABOS_AUDIO_SAMPLE_BITS,
        .channel         = 2U,
        .channel_mask    = 0U,
        .sample_rate     = sample_rate,
    };
    if (esp_codec_dev_open(speaker_codec, &format) != ESP_CODEC_DEV_OK) {
        return false;
    }
    if (esp_codec_dev_open(microphone_codec, &format) != ESP_CODEC_DEV_OK) {
        (void) esp_codec_dev_close(speaker_codec);
        return false;
    }
    if (esp_codec_dev_set_out_vol(speaker_codec, 70) != ESP_CODEC_DEV_OK) {
        (void) esp_codec_dev_close(microphone_codec);
        (void) esp_codec_dev_close(speaker_codec);
        return false;
    }
    atomic_store_explicit(&audio_sample_rate, sample_rate, memory_order_release);
    return true;
}

static bool sample_rate_supported(uint32_t sample_rate)
{
    switch (sample_rate) {
        case TABOS_AUDIO_SAMPLE_RATE_8000:
        case TABOS_AUDIO_SAMPLE_RATE_11025:
        case TABOS_AUDIO_SAMPLE_RATE_12000:
        case TABOS_AUDIO_SAMPLE_RATE_16000:
        case TABOS_AUDIO_SAMPLE_RATE_22050:
        case TABOS_AUDIO_SAMPLE_RATE_24000:
        case TABOS_AUDIO_SAMPLE_RATE_32000:
        case TABOS_AUDIO_SAMPLE_RATE_44100:
        case TABOS_AUDIO_SAMPLE_RATE_48000:
        case TABOS_AUDIO_SAMPLE_RATE_88200:
        case TABOS_AUDIO_SAMPLE_RATE_96000: return true;
        default: return false;
    }
}

bool platform_audio_init(platform_audio_render_fn render, platform_audio_capture_fn capture,
                         platform_audio_error_fn error, platform_audio_info_t* info)
{
    if (render == NULL || capture == NULL || error == NULL || info == NULL) {
        return false;
    }
    *info = (platform_audio_info_t) {
        .driver              = "ES8388/ES7210",
        .features            = TABOS_AUDIO_FEATURE_PLAYBACK | TABOS_AUDIO_FEATURE_CAPTURE,
        .routes              = TABOS_AUDIO_ROUTE_SPEAKER | TABOS_AUDIO_ROUTE_HEADPHONE | TABOS_AUDIO_ROUTE_MICROPHONE,
        .capture_channels    = 2U,
        .sample_rates        = TABOS_AUDIO_RATES_ALL,
        .default_sample_rate = TABOS_AUDIO_DEFAULT_SAMPLE_RATE,
        .detected            = true,
        .ready               = false,
        .error               = EIO,
    };
    route_mutex = xSemaphoreCreateMutex();
    if (route_mutex == NULL) {
        return false;
    }
    speaker_codec    = bsp_audio_codec_speaker_init();
    microphone_codec = bsp_audio_codec_microphone_init();
    if (speaker_codec == NULL || microphone_codec == NULL) {
        platform_audio_shutdown();
        return false;
    }
    if (!configure_codecs(TABOS_AUDIO_DEFAULT_SAMPLE_RATE)) {
        platform_audio_shutdown();
        return false;
    }
    headphone_expander = bsp_io_expander_init();
    bool inserted      = false;
    if (headphone_expander == NULL ||
        esp_io_expander_set_dir(headphone_expander, TAB5_HEADPHONE_DETECT_PIN, IO_EXPANDER_INPUT) != ESP_OK ||
        !read_headphones_inserted(&inserted)) {
        platform_audio_shutdown();
        return false;
    }
    speaker_route_requested = true;
    headphones_inserted     = inserted;
    speaker_enabled         = true;
    xSemaphoreTake(route_mutex, portMAX_DELAY);
    const bool route_ready = apply_output_route_locked();
    xSemaphoreGive(route_mutex);
    if (!route_ready) {
        platform_audio_shutdown();
        return false;
    }
    render_callback  = render;
    capture_callback = capture;
    error_callback   = error;
    if (!start_headphone_monitor() || !start_audio_task()) {
        platform_audio_shutdown();
        return false;
    }
    info->ready = true;
    info->error = 0;
    return true;
}

void platform_audio_shutdown(void)
{
    stop_audio_task();
    stop_headphone_monitor();
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
    if (route_mutex != NULL) {
        vSemaphoreDelete(route_mutex);
        route_mutex = NULL;
    }
    headphone_expander      = NULL;
    speaker_route_requested = false;
    headphones_inserted     = false;
    speaker_enabled         = false;
    render_callback         = NULL;
    capture_callback        = NULL;
    error_callback          = NULL;
    atomic_store_explicit(&audio_sample_rate, 0U, memory_order_release);
}

bool platform_audio_set_sample_rate(uint32_t sample_rate)
{
    if (!sample_rate_supported(sample_rate)) {
        return false;
    }
    const uint32_t previous_sample_rate = atomic_load_explicit(&audio_sample_rate, memory_order_acquire);
    if (sample_rate == previous_sample_rate) {
        return true;
    }
    stop_audio_task();
    (void) esp_codec_dev_close(microphone_codec);
    (void) esp_codec_dev_close(speaker_codec);
    if (configure_codecs(sample_rate) && start_audio_task()) {
        return true;
    }
    (void) esp_codec_dev_close(microphone_codec);
    (void) esp_codec_dev_close(speaker_codec);
    if (configure_codecs(previous_sample_rate) && start_audio_task()) {
        return false;
    }
    error_callback(EIO);
    return false;
}

bool platform_audio_set_route(uint32_t route)
{
    if (route == TABOS_AUDIO_ROUTE_SPEAKER || route == TABOS_AUDIO_ROUTE_HEADPHONE) {
        xSemaphoreTake(route_mutex, portMAX_DELAY);
        const bool previous_route = speaker_route_requested;
        speaker_route_requested   = route == TABOS_AUDIO_ROUTE_SPEAKER;
        const bool configured     = apply_output_route_locked();
        if (!configured) {
            speaker_route_requested = previous_route;
        }
        xSemaphoreGive(route_mutex);
        return configured;
    }
    return route == TABOS_AUDIO_ROUTE_MICROPHONE;
}
