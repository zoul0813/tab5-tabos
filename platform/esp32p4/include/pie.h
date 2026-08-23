#ifndef TABOS_ESP32P4_PIE_H
#define TABOS_ESP32P4_PIE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool esp32p4_pie_fill16(uint16_t *destination, size_t count, const uint16_t *color);
bool esp32p4_pie_copy16(uint16_t *destination, const uint16_t *source, size_t count);

#endif
