#ifndef TABOS_ESP32P4_INTERNAL_H
#define TABOS_ESP32P4_INTERNAL_H

#include <stdbool.h>

bool tab5_keyboard_init(void);
void tab5_keyboard_poll(void);
void tab5_keyboard_shutdown(void);
const char *tab5_keyboard_name(void);
bool tab5_keyboard_present(void);

#endif
