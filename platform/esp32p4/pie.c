#include "include/pie.h"

#include <tabos/config/acceleration.h>
#include <tabos/platform/platform.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PIE_FILL_MIN_PIXELS          16U
#define PIE_COPY_MIN_PIXELS          16U
#define PIE_DIAGNOSTIC_MAX_PIXELS    (64U * 1024U)
#define PIE_DIAGNOSTIC_TARGET_PIXELS (2U * 1024U * 1024U)
#define PIE_DIAGNOSTIC_WARMUP_ROUNDS 4U

#if TABOS_ENABLE_PIE_DIAGNOSTICS
static const char* TAG = "tabos_pie";
#endif

#if TABOS_ENABLE_PIE
void esp32p4_pie_fill16_asm(uint16_t* destination, size_t count, const uint16_t* color);
void esp32p4_pie_copy16_asm(uint16_t* destination, const uint16_t* source, size_t count);
#endif

static bool aligned16(const void* pointer)
{
    return ((uintptr_t) pointer & 15U) == 0U;
}

bool esp32p4_pie_fill16(uint16_t* destination, size_t count, const uint16_t* color)
{
#if TABOS_ENABLE_PIE
    if (destination == NULL || color == NULL || count < PIE_FILL_MIN_PIXELS || !aligned16(destination)) {
        return false;
    }
    esp32p4_pie_fill16_asm(destination, count, color);
    return true;
#else
    (void) destination;
    (void) count;
    (void) color;
    return false;
#endif
}

bool esp32p4_pie_copy16(uint16_t* destination, const uint16_t* source, size_t count)
{
#if TABOS_ENABLE_PIE
    if (destination == NULL || source == NULL || count < PIE_COPY_MIN_PIXELS || !aligned16(destination) ||
        !aligned16(source)) {
        return false;
    }
    esp32p4_pie_copy16_asm(destination, source, count);
    return true;
#else
    (void) destination;
    (void) source;
    (void) count;
    return false;
#endif
}

bool platform_raster_fill_span(platform_pixel_t* destination, size_t count, platform_pixel_t color)
{
    return esp32p4_pie_fill16(destination, count, &color);
}

bool platform_raster_copy_span(platform_pixel_t* destination, const platform_pixel_t* source, size_t count)
{
    return esp32p4_pie_copy16(destination, source, count);
}

#if TABOS_ENABLE_PIE_DIAGNOSTICS
static uint32_t hash_pixels(const uint16_t* pixels, size_t count)
{
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0U; index < count; ++index) {
        hash ^= pixels[index];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

static void scalar_fill(uint16_t* destination, size_t count, uint16_t color)
{
    for (size_t index = 0U; index < count; ++index) {
        destination[index] = color;
    }
}

typedef struct {
        size_t pixels;
        const char* workload;
} pie_diagnostic_size_t;

static const pie_diagnostic_size_t diagnostic_sizes[] = {
    {    8U,     "glyph-run"},
    {   16U,     "glyph-run"},
    {   32U,          "span"},
    {   64U,          "span"},
    {  256U,          "span"},
    {  384U, "terminal-cell"},
    { 1024U,          "span"},
    { 4096U,  "small-sprite"},
    {65536U,  "large-region"},
};

static void benchmark_size(const char* placement, uint16_t* source, uint16_t* destination,
                           const pie_diagnostic_size_t* test)
{
    const size_t pixels = test->pixels;
    const size_t bytes  = pixels * sizeof(uint16_t);
    const size_t rounds = (PIE_DIAGNOSTIC_TARGET_PIXELS + pixels - 1U) / pixels;
    for (size_t index = 0U; index < pixels; ++index) {
        source[index] = (uint16_t) (index * 40503U);
    }
    for (unsigned int round = 0U; round < PIE_DIAGNOSTIC_WARMUP_ROUNDS; ++round) {
        scalar_fill(destination, pixels, UINT16_C(0x5aa5));
        (void) esp32p4_pie_fill16(destination, pixels, &(const uint16_t) {UINT16_C(0x5aa5)});
        memcpy(destination, source, bytes);
        (void) esp32p4_pie_copy16(destination, source, pixels);
    }
    int64_t started = esp_timer_get_time();
    for (size_t round = 0U; round < rounds; ++round) {
        scalar_fill(destination, pixels, UINT16_C(0x5aa5));
    }
    const int64_t scalar_fill_us      = esp_timer_get_time() - started;
    const uint32_t expected_fill_hash = hash_pixels(destination, pixels);
    started                           = esp_timer_get_time();
    for (size_t round = 0U; round < rounds; ++round) {
        (void) esp32p4_pie_fill16(destination, pixels, &(const uint16_t) {UINT16_C(0x5aa5)});
    }
    const int64_t pie_fill_us = esp_timer_get_time() - started;
    const uint32_t fill_hash  = hash_pixels(destination, pixels);
    started                   = esp_timer_get_time();
    for (size_t round = 0U; round < rounds; ++round) {
        memcpy(destination, source, bytes);
    }
    const int64_t scalar_copy_us = esp_timer_get_time() - started;
    started                      = esp_timer_get_time();
    for (size_t round = 0U; round < rounds; ++round) {
        (void) esp32p4_pie_copy16(destination, source, pixels);
    }
    const int64_t pie_copy_us       = esp_timer_get_time() - started;
    const uint32_t copy_hash        = hash_pixels(source, pixels);
    const uint32_t output_copy_hash = hash_pixels(destination, pixels);
    const double mib                = (double) bytes * (double) rounds / (1024.0 * 1024.0);
    ESP_LOGI(TAG, "%s fill %u px (%s): scalar=%" PRId64 " us PIE=%" PRId64 " us %.2fx %.1f MiB/s hash=%08" PRIx32 " %s",
             placement, (unsigned int) pixels, test->workload, scalar_fill_us, pie_fill_us,
             pie_fill_us > 0 ? (double) scalar_fill_us / (double) pie_fill_us : 0.0,
             pie_fill_us > 0 ? mib * 1000000.0 / (double) pie_fill_us : 0.0, fill_hash,
             fill_hash == expected_fill_hash ? "MATCH" : "MISMATCH");
    ESP_LOGI(TAG, "%s copy %u px (%s): libc=%" PRId64 " us PIE=%" PRId64 " us %.2fx %.1f MiB/s hash=%08" PRIx32 " %s",
             placement, (unsigned int) pixels, test->workload, scalar_copy_us, pie_copy_us,
             pie_copy_us > 0 ? (double) scalar_copy_us / (double) pie_copy_us : 0.0,
             pie_copy_us > 0 ? mib * 1000000.0 / (double) pie_copy_us : 0.0, output_copy_hash,
             output_copy_hash == copy_hash ? "MATCH" : "MISMATCH");
}

static void benchmark_memory(const char* placement, uint32_t caps)
{
    const size_t bytes    = PIE_DIAGNOSTIC_MAX_PIXELS * sizeof(uint16_t);
    uint16_t* source      = heap_caps_aligned_alloc(16U, bytes, caps);
    uint16_t* destination = heap_caps_aligned_alloc(16U, bytes, caps);
    if (source == NULL || destination == NULL) {
        ESP_LOGW(TAG, "%s diagnostics skipped: allocation failed", placement);
        free(source);
        free(destination);
        return;
    }
    for (size_t index = 0U; index < sizeof(diagnostic_sizes) / sizeof(diagnostic_sizes[0]); ++index) {
        benchmark_size(placement, source, destination, &diagnostic_sizes[index]);
    }
    free(source);
    free(destination);
}
#endif

void platform_raster_diagnostics(void)
{
#if TABOS_ENABLE_PIE_DIAGNOSTICS
    ESP_LOGI(TAG, "PIE diagnostics: aligned size sweep; about %u pixels per sample", PIE_DIAGNOSTIC_TARGET_PIXELS);
    benchmark_memory("internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    benchmark_memory("PSRAM", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
}
