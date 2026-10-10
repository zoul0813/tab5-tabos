#include <soccer/sound.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <tabos/runtime_time.h>

#include "../assets/sound_pcm.inc"

void soccer_sound_close(soccer_sound_t* sound)
{
    sound->active_effect      = SOCCER_EVENT_NONE;
    sound->effect_duration_ms = 0U;
    if (sound->stream != TABOS_AUDIO_STREAM_INVALID) {
        if (tabos_audio_close(sound->stream) != 0 && sound->error == 0) {
            sound->error = errno;
        }
        sound->stream = TABOS_AUDIO_STREAM_INVALID;
    }
}

void soccer_sound_stop(soccer_sound_t* sound)
{
    soccer_sound_close(sound);
}

void soccer_sound_prepare(soccer_sound_t* sound)
{
    if (sound->muted || (sound->stream != TABOS_AUDIO_STREAM_INVALID && sound->error == 0)) {
        return;
    }
    /* Device startup/recovery belongs at a UI transition, before the match clock resumes. */
    soccer_sound_close(sound);
    sound->error                      = 0;
    const tabos_audio_config_t config = {
        .direction   = TABOS_AUDIO_PLAYBACK,
        .channels    = 1U,
        .route       = TABOS_AUDIO_ROUTE_SPEAKER,
        .sample_rate = 44100U,
    };
    sound->stream = tabos_audio_open(&config);
    if (sound->stream == TABOS_AUDIO_STREAM_INVALID) {
        sound->error = errno;
    }
}

void soccer_sound_toggle(soccer_sound_t* sound)
{
    sound->muted = !sound->muted;
    if (sound->muted) {
        soccer_sound_close(sound);
    }
}

void soccer_sound_play(soccer_sound_t* sound, soccer_event_t effect)
{
    static const int16_t* const pcm[] = {NULL,     effect_1, effect_2, effect_3, effect_4,
                                         effect_5, effect_6, effect_7, effect_8, effect_9};
    static const uint32_t bytes[]     = {0U,
                                         sizeof(effect_1),
                                         sizeof(effect_2),
                                         sizeof(effect_3),
                                         sizeof(effect_4),
                                         sizeof(effect_5),
                                         sizeof(effect_6),
                                         sizeof(effect_7),
                                         sizeof(effect_8),
                                         sizeof(effect_9)};
    _Static_assert(sizeof(pcm) / sizeof(pcm[0]) == SOCCER_EVENT_FINISH + 1U, "sound effects");
    _Static_assert(sizeof(bytes) / sizeof(bytes[0]) == SOCCER_EVENT_FINISH + 1U, "sound lengths");
    if (sound->muted || effect <= SOCCER_EVENT_NONE || effect > SOCCER_EVENT_FINISH ||
        sound->stream == TABOS_AUDIO_STREAM_INVALID || sound->error != 0) {
        return;
    }
    const uint64_t now = tabos_monotonic_ms();
    if (now - sound->effect_started_ms < sound->effect_duration_ms && effect <= sound->active_effect) {
        return;
    }
    if (tabos_audio_flush(sound->stream) != 0) {
        sound->error = errno;
        return;
    }
    uint32_t written = 0U;
    for (unsigned int attempt = 0U; attempt < 8U && written < bytes[effect]; ++attempt) {
        const int count =
            tabos_audio_write(sound->stream, (const uint8_t*) pcm[effect] + written, bytes[effect] - written);
        if (count <= 0 || (uint32_t) count > bytes[effect] - written || count % 2 != 0) {
            sound->error = count < 0 ? errno : EIO;
            return;
        }
        written += (uint32_t) count;
    }
    if (written != bytes[effect]) {
        sound->error = EAGAIN;
    } else {
        sound->active_effect      = effect;
        sound->effect_started_ms  = now;
        sound->effect_duration_ms = bytes[effect] * 1000U / (44100U * sizeof(int16_t));
    }
    /* Keep queued audio alive on backpressure. Last-stream teardown can block
       and discard a short sound; cleanup/retry happens on pause, mute or exit. */
}
