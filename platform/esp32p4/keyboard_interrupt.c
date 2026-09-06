#include "keyboard_interrupt.h"

#include <stddef.h>

#define KEYBOARD_NORMAL_INTERRUPT 0x01U
#define KEYBOARD_MAX_EVENTS       32U

bool tab5_keyboard_interrupt_drain(const tab5_keyboard_interrupt_ops_t* ops, unsigned int maximum_passes,
                                   bool* still_pending)
{
    if (ops == NULL || ops->read_status == NULL || ops->read_count == NULL || ops->read_event == NULL ||
        ops->clear_status == NULL || ops->interrupt_asserted == NULL || ops->submit_event == NULL ||
        maximum_passes == 0U || still_pending == NULL) {
        return false;
    }

    *still_pending = false;
    for (unsigned int pass = 0U; pass < maximum_passes; ++pass) {
        uint8_t status = 0U;
        uint8_t count  = 0U;
        if (!ops->read_status(ops->context, &status) || !ops->read_count(ops->context, &count)) {
            return false;
        }
        if (count > KEYBOARD_MAX_EVENTS) {
            count = KEYBOARD_MAX_EVENTS;
        }
        for (uint8_t index = 0U; index < count; ++index) {
            uint8_t event = 0xffU;
            if (!ops->read_event(ops->context, &event)) {
                return false;
            }
            if (event != 0xffU) {
                ops->submit_event(ops->context, event);
            }
        }
        if (!ops->clear_status(ops->context) || !ops->read_status(ops->context, &status)) {
            return false;
        }
        *still_pending = (status & KEYBOARD_NORMAL_INTERRUPT) != 0U || ops->interrupt_asserted(ops->context);
        if (!*still_pending) {
            return true;
        }
    }
    return true;
}
