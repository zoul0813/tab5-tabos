#ifndef TABOS_ESP32P4_INTERNAL_H
#define TABOS_ESP32P4_INTERNAL_H

#include <stdbool.h>

bool tab_esp32p4_keyboard_init(void);
void tab_esp32p4_keyboard_poll(void);
void tab_esp32p4_keyboard_shutdown(void);
const char *tab_esp32p4_keyboard_name(void);
bool tab_esp32p4_keyboard_present(void);

#endif
