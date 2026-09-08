#pragma once

typedef enum {
    TAB5_ACTIVITY_AUDIO_CHUNKS,
    TAB5_ACTIVITY_AUDIO_FRAMES,
    TAB5_ACTIVITY_AUDIO_ERRORS,
    TAB5_ACTIVITY_HEADPHONE_READS,
    TAB5_ACTIVITY_HEADPHONE_ERRORS,
    TAB5_ACTIVITY_VSYNC,
    TAB5_ACTIVITY_PPA,
    TAB5_ACTIVITY_COUNT
} tab5_activity_counter_t;

#ifndef NDEBUG
void tab5_activity_record(tab5_activity_counter_t counter, unsigned int amount);
#else
#define tab5_activity_record(counter, amount) ((void) 0)
#endif
