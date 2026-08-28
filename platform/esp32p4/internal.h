#ifndef TABOS_ESP32P4_INTERNAL_H
#define TABOS_ESP32P4_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

bool tab5_keyboard_init(void);
void tab5_keyboard_poll(void);
void tab5_keyboard_shutdown(void);
const char* tab5_keyboard_name(void);
bool tab5_keyboard_present(void);
bool tab5_keyboard_delete_held(uint32_t window_ms);
bool platform_usb_port_disable_host_power(void);
bool tab5_rtc_init(void);
void tab5_rtc_shutdown(void);
bool tab5_rtc_present(void);

#endif
