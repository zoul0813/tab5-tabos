#include "msc_control.h"
#include "../../../platform/esp32p4/msc_protocol.h"
#include <tabos/platform/platform.h>
#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <stdatomic.h>
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
#include "../../../platform/esp32p4/device_test_protocol.h"
#include <tabos/internal/input.h>
#include <tabos/internal/console.h>
#include <tabos/internal/application.h>
#include <tabos/platform/esp32p4.h>
#include <stdio.h>
#include <stdlib.h>
#include <sdkconfig.h>
#include <esp_heap_caps.h>
#include <string.h>

static bool testing;
static atomic_bool status_requested;
static atomic_uint process_count;
static SemaphoreHandle_t status_ready;

void tab5_test_control_update(void)
{
    if (atomic_exchange_explicit(&status_requested, false, memory_order_acquire)) {
        atomic_store_explicit(&process_count, (unsigned) tabos_process_count(), memory_order_release);
        xSemaphoreGive(status_ready);
    }
}

static unsigned test_process_count(void)
{
    while (xSemaphoreTake(status_ready, 0) == pdTRUE) {}
    atomic_store_explicit(&status_requested, true, memory_order_release);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
    if (xSemaphoreTake(status_ready, pdMS_TO_TICKS(3000)) != pdTRUE) {
        return 0U;
    }
    return atomic_load_explicit(&process_count, memory_order_acquire);
}
static bool injected_keys[TABOS_INPUT_KEY_COUNT];
static TickType_t last_input;

static void release_injected_keys(void)
{
    for (size_t key = 0U; key < TABOS_INPUT_KEY_COUNT; ++key) {
        if (injected_keys[key]) {
            tabos_input_event_t event = {.type = TABOS_INPUT_KEY_UP, .key = (tabos_key_t) key};
            (void) input_submit(&event);
            injected_keys[key] = false;
        }
    }
}
static void test_command(const char* line)
{
    if (strcmp(line, "BEGIN") == 0) {
        if (test_process_count() != 1U) {
            puts("TABOS TEST ERROR busy");
            return;
        }
        tab5_test_display_reset();
        testing = console_capture_start();
    } else if (!testing) {
        puts("TABOS TEST ERROR disabled");
        return;
    } else if (strcmp(line, "END") == 0) {
        release_injected_keys();
        console_capture_stop();
        testing = false;
    } else if (strcmp(line, "STATUS") == 0) {
        printf("TABOS TEST STATUS %u\n", test_process_count());
        return;
    } else if (strcmp(line, "MUTE 0") == 0 || strcmp(line, "MUTE 1") == 0) {
        if (test_process_count() != 1U) {
            puts("TABOS TEST ERROR mute-require-shell");
            return;
        }
        tab5_test_audio_set_muted(line[5] == '1');
    } else if (strcmp(line, "STATS") == 0) {
        if (test_process_count() != 1U) {
            puts("TABOS TEST ERROR stats-require-shell");
            return;
        }
        const tab5_test_display_stats_t stats = tab5_test_display_stats();
        platform_diagnostics_t diagnostics    = {0};
        (void) platform_get_diagnostics(&diagnostics);
        printf("TABOS TEST STATS clear_us=%llu blit_us=%llu overlay_us=%llu submit_us=%llu frames=%u accelerated=%u "
               "software=%u cpu_mhz=%u configured_cpu_mhz=%u cache_kib=%u internal_free=%u internal_min_free=%u "
               "internal_largest=%u worker_us=%llu codec_muted=%u\n",
               (unsigned long long) stats.clear_us, (unsigned long long) stats.blit_us,
               (unsigned long long) stats.overlay_us, (unsigned long long) stats.submit_us, (unsigned) stats.frames,
               (unsigned) stats.accelerated_blits, (unsigned) stats.software_blits, diagnostics.cpu_frequency_mhz,
               (unsigned) CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, (unsigned) CONFIG_CACHE_L2_CACHE_SIZE / 1024U,
               (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned long long) stats.worker_us, (unsigned) tab5_test_audio_muted());
        return;
    } else if (strcmp(line, "READ") == 0) {
        char output[256];
        bool overflow;
        const size_t count = console_capture_read(output, sizeof(output), &overflow);
        if (overflow) {
            puts("TABOS TEST ERROR output-overflow");
            return;
        }
        char hex[513];
        for (size_t i = 0U; i < count; ++i) {
            static const char digits[] = "0123456789abcdef";
            hex[i * 2U]                = digits[(unsigned char) output[i] >> 4U];
            hex[i * 2U + 1U]           = digits[(unsigned char) output[i] & 15U];
        }
        hex[count * 2U] = '\0';
        printf("TABOS TEST DATA %s\n", hex);
        return;
    } else if (strcmp(line, "SHOT") == 0) {
        uint16_t* pixels = tab5_test_capture_display();
        if (pixels == NULL) {
            puts("TABOS TEST ERROR screenshot-timeout");
            return;
        }
        puts("TABOS TEST IMAGE 640 360");
        char hex[2561];
        static const char digits[] = "0123456789abcdef";
        for (size_t row = 0U; row < 360U; ++row) {
            const uint16_t* source = pixels + row * 640U;
            size_t runs            = 1U;
            for (size_t x = 1U; x < 640U; ++x) {
                runs += source[x] != source[x - 1U];
            }
            size_t used = 0U;
            for (size_t x = 0U; x < 640U;) {
                size_t count = 1U;
                if (runs <= 320U) {
                    while (x + count < 640U && source[x + count] == source[x]) {
                        ++count;
                    }
                    for (size_t nibble = 0U; nibble < 4U; ++nibble) {
                        hex[used++] = digits[(count >> (12U - nibble * 4U)) & 15U];
                    }
                }
                for (size_t nibble = 0U; nibble < 4U; ++nibble) {
                    hex[used++] = digits[(source[x] >> (12U - nibble * 4U)) & 15U];
                }
                x += count;
            }
            hex[used] = '\0';
            printf("TABOS TEST %s %s\n", runs <= 320U ? "RLE" : "ROW", hex);
            /* USB output can remain runnable for a large capture. Give idle
             * reclamation/watchdog work a bounded window on either core. */
            if ((row + 1U) % 16U == 0U) {
                vTaskDelay(1U);
            }
        }
        free(pixels);
    } else {
        tab5_test_input_t input;
        if (!tab5_test_parse_input(line, &input)) {
            puts("TABOS TEST ERROR invalid-input");
            return;
        }
        tabos_input_event_t event = {.type = TABOS_INPUT_TEXT};
        if (input.text[0] != '\0') {
            if (test_process_count() != 1U) {
                puts("TABOS TEST ERROR text-requires-shell");
                return;
            }
            memcpy(event.text, input.text, sizeof(event.text));
        } else {
            event.type              = input.down ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP;
            event.key               = (tabos_key_t) input.key;
            event.modifiers         = (uint8_t) input.modifiers;
            event.logical_key       = event.key;
            event.logical_modifiers = event.modifiers;
        }
        if (!input_submit(&event)) {
            puts("TABOS TEST ERROR input-unavailable");
            return;
        }
        if (event.type != TABOS_INPUT_TEXT) {
            injected_keys[input.key] = input.down != 0U;
            last_input               = xTaskGetTickCount();
        }
        if (event.type == TABOS_INPUT_KEY_DOWN && input.key < 256U &&
            input_text_from_hid((uint8_t) input.key, event.modifiers, event.text, sizeof(event.text)) != 0U) {
            event.type = TABOS_INPUT_TEXT;
            (void) input_submit(&event);
        }
    }
    puts(testing || strcmp(line, "END") == 0 ? "TABOS TEST OK" : "TABOS TEST ERROR capture-allocation");
}
#endif

static TaskHandle_t control_task;
static atomic_bool requested;

static void receive_commands(void* unused)
{
    (void) unused;
    tab5_msc_command_t command = {0};
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    char line[96];
    size_t used   = 0U;
    bool overflow = false;
#endif
    for (;;) {
        char byte;
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        if ((TickType_t) (xTaskGetTickCount() - last_input) >= pdMS_TO_TICKS(2000)) {
            release_injected_keys();
        }
#endif
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        const TickType_t receive_timeout = pdMS_TO_TICKS(100);
#else
        const TickType_t receive_timeout = portMAX_DELAY;
#endif
        if (usb_serial_jtag_read_bytes(&byte, 1U, receive_timeout) != 1) {
            continue;
        }
        if (tab5_msc_command_feed(&command, byte)) {
            atomic_store_explicit(&requested, true, memory_order_release);
            platform_runtime_notify(PLATFORM_RUNTIME_EVENT_APPLICATION);
        }
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        if (byte == '\r' || byte == '\n') {
            line[used] = '\0';
            if (!overflow && strncmp(line, "TABOS TEST ", 11U) == 0) {
                test_command(line + 11U);
                (void) fflush(stdout);
            }
            used     = 0U;
            overflow = false;
        } else if (byte == '\0') {
            overflow = true;
        } else if (used < sizeof(line) - 1U) {
            line[used++] = byte;
        } else {
            overflow = true;
        }
#endif
    }
}

bool tab5_msc_control_start(void)
{
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
    status_ready = xSemaphoreCreateBinary();
    if (status_ready == NULL) {
        return false;
    }
#endif
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        vSemaphoreDelete(status_ready);
        status_ready = NULL;
#endif
        return false;
    }
    if (xTaskCreate(receive_commands, "msc-control", 8192U, NULL, 3U, &control_task) != pdPASS) {
        (void) usb_serial_jtag_driver_uninstall();
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        vSemaphoreDelete(status_ready);
        status_ready = NULL;
#endif
        return false;
    }
    return true;
}

void tab5_msc_control_stop(void)
{
    if (control_task != NULL) {
        vTaskDelete(control_task);
        control_task = NULL;
        (void) usb_serial_jtag_driver_uninstall();
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
        release_injected_keys();
        console_capture_stop();
        vSemaphoreDelete(status_ready);
        status_ready = NULL;
#endif
    }
}

bool tab5_msc_control_take_request(void)
{
    return atomic_exchange_explicit(&requested, false, memory_order_acq_rel);
}
