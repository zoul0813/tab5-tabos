#include "touch_interrupt.h"

#include <stddef.h>
#include <string.h>

static void reconcile_points(tab5_touch_interrupt_state_t* state, const tab5_touch_interrupt_ops_t* ops,
                             const tab5_touch_point_t* points, uint8_t point_count)
{
    bool matched_contacts[TABOS_POINTER_MAX_CONTACTS] = {false};
    bool matched_points[TABOS_POINTER_MAX_CONTACTS]   = {false};
    for (uint8_t point = 0U; point < point_count; ++point) {
        uint32_t best_contact  = TABOS_POINTER_MAX_CONTACTS;
        uint64_t best_distance = UINT64_MAX;
        for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
            if (!state->contacts[contact].active || matched_contacts[contact]) {
                continue;
            }
            const int64_t dx        = (int64_t) state->contacts[contact].x - points[point].x;
            const int64_t dy        = (int64_t) state->contacts[contact].y - points[point].y;
            const uint64_t distance = (uint64_t) (dx * dx + dy * dy);
            if (distance < best_distance) {
                best_distance = distance;
                best_contact  = contact;
            }
        }
        if (best_contact < TABOS_POINTER_MAX_CONTACTS) {
            matched_contacts[best_contact] = true;
            matched_points[point]          = true;
            const bool moved               = state->contacts[best_contact].x != points[point].x ||
                               state->contacts[best_contact].y != points[point].y;
            state->contacts[best_contact].x = points[point].x;
            state->contacts[best_contact].y = points[point].y;
            if (moved) {
                ops->submit_contact(ops->context, TABOS_POINTER_MOVE, best_contact, points[point].x, points[point].y);
            }
        }
    }
    for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
        if (state->contacts[contact].active && !matched_contacts[contact]) {
            ops->submit_contact(ops->context, TABOS_POINTER_UP, contact, state->contacts[contact].x,
                                state->contacts[contact].y);
            state->contacts[contact].active = false;
        }
    }
    for (uint8_t point = 0U; point < point_count; ++point) {
        if (matched_points[point]) {
            continue;
        }
        for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
            if (state->contacts[contact].active) {
                continue;
            }
            state->contacts[contact] = (tab5_touch_contact_t) {
                .x      = points[point].x,
                .y      = points[point].y,
                .active = true,
            };
            ops->submit_contact(ops->context, TABOS_POINTER_DOWN, contact, points[point].x, points[point].y);
            break;
        }
    }
}

void tab5_touch_interrupt_state_init(tab5_touch_interrupt_state_t* state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

bool tab5_touch_interrupt_drain(tab5_touch_interrupt_state_t* state, const tab5_touch_interrupt_ops_t* ops,
                                unsigned int maximum_passes, bool* still_pending)
{
    if (state == NULL || ops == NULL || ops->read_points == NULL || ops->interrupt_asserted == NULL ||
        ops->submit_contact == NULL || maximum_passes == 0U || still_pending == NULL) {
        return false;
    }

    *still_pending = false;
    for (unsigned int pass = 0U; pass < maximum_passes; ++pass) {
        tab5_touch_point_t points[TABOS_POINTER_MAX_CONTACTS];
        uint8_t point_count = 0U;
        if (!ops->read_points(ops->context, points, &point_count, TABOS_POINTER_MAX_CONTACTS) ||
            point_count > TABOS_POINTER_MAX_CONTACTS) {
            tab5_touch_interrupt_cancel(state, ops);
            return false;
        }
        reconcile_points(state, ops, points, point_count);
        *still_pending = ops->interrupt_asserted(ops->context);
        if (!*still_pending) {
            return true;
        }
    }
    return true;
}

void tab5_touch_interrupt_cancel(tab5_touch_interrupt_state_t* state, const tab5_touch_interrupt_ops_t* ops)
{
    if (state == NULL || ops == NULL || ops->submit_contact == NULL) {
        return;
    }
    for (uint32_t contact = 0U; contact < TABOS_POINTER_MAX_CONTACTS; ++contact) {
        if (!state->contacts[contact].active) {
            continue;
        }
        ops->submit_contact(ops->context, TABOS_POINTER_CANCEL, contact, state->contacts[contact].x,
                            state->contacts[contact].y);
        state->contacts[contact].active = false;
    }
}
