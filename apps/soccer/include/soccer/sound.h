#ifndef SOCCER_SOUND_H
#define SOCCER_SOUND_H

#include <soccer/game.h>
#include <tabos/audio.h>

typedef struct {
        tabos_audio_stream_t stream;
        soccer_event_t active_effect;
        uint64_t effect_started_ms;
        uint32_t effect_duration_ms;
        bool muted;
        int error;
} soccer_sound_t;

void soccer_sound_prepare(soccer_sound_t* sound);
void soccer_sound_play(soccer_sound_t* sound, soccer_event_t effect);
void soccer_sound_stop(soccer_sound_t* sound);
void soccer_sound_toggle(soccer_sound_t* sound);
void soccer_sound_close(soccer_sound_t* sound);

#endif
