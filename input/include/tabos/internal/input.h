#ifndef TABOS_INTERNAL_INPUT_H
#define TABOS_INTERNAL_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/input.h>

void tab_input_init(void);
void tab_input_shutdown(void);
bool tab_input_submit(const tabos_input_event_t *event);
size_t tab_input_text_from_hid(uint8_t usage, uint8_t modifiers, char *text, size_t text_size);

#endif
