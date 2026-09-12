#ifndef SNAKE_SOUND_H
#define SNAKE_SOUND_H

#include <stdbool.h>
#include <tabos/audio.h>

typedef enum {
    SNAKE_SOUND_START,
    SNAKE_SOUND_EAT,
    SNAKE_SOUND_LOSE,
    SNAKE_SOUND_WIN
} snake_sound_effect_t;

typedef struct {
        tabos_audio_stream_t stream;
        bool muted;
} snake_sound_t;

void snake_sound_play(snake_sound_t* sound, snake_sound_effect_t effect);
void snake_sound_stop(snake_sound_t* sound);
void snake_sound_toggle(snake_sound_t* sound);
void snake_sound_close(snake_sound_t* sound);

#endif
