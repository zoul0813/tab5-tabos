#ifndef TABOS_INTERNAL_INPUT_H
#define TABOS_INTERNAL_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/input.h>

void input_init(void);
void input_shutdown(void);
void input_update(void);
bool input_submit(const tabos_input_event_t* event);
void input_diagnostic_log(const tabos_input_event_t* event);
size_t input_text_from_hid(uint8_t usage, uint8_t modifiers, char* text, size_t text_size);

#endif
