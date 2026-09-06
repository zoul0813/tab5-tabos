#ifndef TABOS_ESP32P4_KEYBOARD_INTERRUPT_H
#define TABOS_ESP32P4_KEYBOARD_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
        void* context;
        bool (*read_status)(void* context, uint8_t* status);
        bool (*read_count)(void* context, uint8_t* count);
        bool (*read_event)(void* context, uint8_t* event);
        bool (*clear_status)(void* context);
        bool (*interrupt_asserted)(void* context);
        void (*submit_event)(void* context, uint8_t event);
} tab5_keyboard_interrupt_ops_t;

bool tab5_keyboard_interrupt_drain(const tab5_keyboard_interrupt_ops_t* ops, unsigned int maximum_passes,
                                   bool* still_pending);

#endif
