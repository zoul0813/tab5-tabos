#ifndef POOL_SOUND_H
#define POOL_SOUND_H
#include <pool/game.h>
#include <tabos/audio.h>

typedef enum {
    POOL_SOUND_NONE,
    POOL_SOUND_CUE,
    POOL_SOUND_BALL,
    POOL_SOUND_RAIL,
    POOL_SOUND_POT,
    POOL_SOUND_FOUL,
    POOL_SOUND_RESULT
} pool_sound_effect_t;
typedef struct {
        tabos_audio_stream_t stream;
        int error;
} pool_sound_t;
void pool_sound_close(pool_sound_t* sound);
/* Open only at explicit start/resume/unmute, never as part of a shot. */
void pool_sound_prepare(pool_sound_t* sound);
void pool_sound_play(pool_sound_t* sound, pool_sound_effect_t effect);
/* Observes a transition; does not alter game state or invent collision events. */
pool_sound_effect_t pool_sound_event(const pool_game_t* before, const pool_game_t* after);
#endif
