#ifndef KILO_INPUT_H
#define KILO_INPUT_H
#include <kilo/editor.h>
#include <tabos/input.h>
typedef enum {
    KILO_IDLE,
    KILO_REDRAW,
    KILO_SAVE,
    KILO_RESIZE
} kilo_action_t;
kilo_action_t kilo_input(kilo_editor_t* editor, const tabos_input_event_t* event);
#endif
