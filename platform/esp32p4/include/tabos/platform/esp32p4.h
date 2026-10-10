#ifndef TABOS_PLATFORM_ESP32P4_H
#define TABOS_PLATFORM_ESP32P4_H

#include <stdbool.h>
#include <stdint.h>

bool tab5_boot_usb_storage_requested(uint32_t window_ms);
void tab5_usb_storage_request_next_boot(void);
bool tab5_usb_storage_start(void);

#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
uint16_t* tab5_test_capture_display(void);
/* Shell-only test command selects codec mute for subsequent audio opens. */
void tab5_test_audio_set_muted(bool muted);
bool tab5_test_audio_muted(void);
typedef struct {
        uint64_t clear_us, blit_us, overlay_us, submit_us, worker_us;
        uint32_t frames, accelerated_blits, software_blits;
} tab5_test_display_stats_t;
void tab5_test_display_reset(void);
tab5_test_display_stats_t tab5_test_display_stats(void);
#endif

#endif
