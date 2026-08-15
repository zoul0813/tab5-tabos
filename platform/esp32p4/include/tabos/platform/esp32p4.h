#ifndef TABOS_PLATFORM_ESP32P4_H
#define TABOS_PLATFORM_ESP32P4_H

#include <stdbool.h>
#include <stdint.h>

bool tab5_boot_usb_storage_requested(uint32_t window_ms);
bool tab5_usb_storage_start(void);

#endif
