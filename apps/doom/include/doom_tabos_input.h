#ifndef DOOM_TABOS_INPUT_H
#define DOOM_TABOS_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/input.h>

enum {
    DOOM_TABOS_INPUT_QUEUE_CAPACITY = 32U,
    DOOM_TABOS_INPUT_KEY_COUNT      = 256U,
    DOOM_TABOS_SOURCE_KEY_COUNT     = TABOS_KEY_SYM + 1U,
};

typedef struct {
        bool pressed;
        unsigned char key;
} doom_tabos_key_event_t;

typedef struct {
        doom_tabos_key_event_t queue[DOOM_TABOS_INPUT_QUEUE_CAPACITY];
        bool source_down[DOOM_TABOS_SOURCE_KEY_COUNT];
        bool logical_down[DOOM_TABOS_INPUT_KEY_COUNT];
        bool delivered_down[DOOM_TABOS_INPUT_KEY_COUNT];
        size_t queue_head;
        size_t queue_count;
        size_t recovery_key;
        bool control_down;
        bool shift_down;
        bool always_run;
        bool recovering;
} doom_tabos_input_t;

void doom_tabos_input_init(doom_tabos_input_t* input);
void doom_tabos_input_feed(doom_tabos_input_t* input, const tabos_input_event_t* event);
bool doom_tabos_input_pop(doom_tabos_input_t* input, int* pressed, unsigned char* key);

#endif
