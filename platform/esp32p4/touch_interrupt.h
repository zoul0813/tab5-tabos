#ifndef TABOS_ESP32P4_TOUCH_INTERRUPT_H
#define TABOS_ESP32P4_TOUCH_INTERRUPT_H

#include <tabos/pointer.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
        uint16_t x;
        uint16_t y;
} tab5_touch_point_t;

typedef struct {
        uint16_t x;
        uint16_t y;
        bool active;
} tab5_touch_contact_t;

typedef struct {
        tab5_touch_contact_t contacts[TABOS_POINTER_MAX_CONTACTS];
} tab5_touch_interrupt_state_t;

typedef struct {
        void* context;
        bool (*read_points)(void* context, tab5_touch_point_t* points, uint8_t* point_count, uint8_t maximum_points);
        bool (*interrupt_asserted)(void* context);
        void (*submit_contact)(void* context, tabos_pointer_event_type_t type, uint32_t contact, uint16_t x,
                               uint16_t y);
} tab5_touch_interrupt_ops_t;

void tab5_touch_interrupt_state_init(tab5_touch_interrupt_state_t* state);
bool tab5_touch_interrupt_drain(tab5_touch_interrupt_state_t* state, const tab5_touch_interrupt_ops_t* ops,
                                unsigned int maximum_passes, bool* still_pending);
void tab5_touch_interrupt_cancel(tab5_touch_interrupt_state_t* state, const tab5_touch_interrupt_ops_t* ops);

#endif
