#include <pool/sound.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#include "../assets/sound_pcm.inc"

void pool_sound_close(pool_sound_t* sound)
{
    if (sound->stream != TABOS_AUDIO_STREAM_INVALID) {
        if (tabos_audio_close(sound->stream) != 0 && sound->error == 0) {
            sound->error = errno;
        }
        sound->stream = TABOS_AUDIO_STREAM_INVALID;
    }
}

void pool_sound_prepare(pool_sound_t* sound)
{
    if (sound->stream != TABOS_AUDIO_STREAM_INVALID) {
        return;
    }
    sound->error                      = 0;
    const tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_PLAYBACK, .channels = 1U, .route = TABOS_AUDIO_ROUTE_SPEAKER, .sample_rate = 44100U};
    sound->stream = tabos_audio_open(&config);
    if (sound->stream == TABOS_AUDIO_STREAM_INVALID) {
        sound->error = errno;
    }
}

void pool_sound_play(pool_sound_t* sound, pool_sound_effect_t effect)
{
    static const int16_t* const pcm[] = {NULL, effect_1, effect_2, effect_3, effect_4, effect_5, effect_6};
    static const uint32_t bytes[]     = {
        0U, sizeof(effect_1), sizeof(effect_2), sizeof(effect_3), sizeof(effect_4), sizeof(effect_5), sizeof(effect_6)};
    if (effect <= POOL_SOUND_NONE || effect > POOL_SOUND_RESULT || sound->stream == TABOS_AUDIO_STREAM_INVALID ||
        sound->error != 0) {
        return;
    }
    /* Reuse the running device. Neither codec startup, PCM synthesis nor device
     * teardown belongs on the shot path. A short effect must not be closed
     * against a deadline measured before device startup/queueing. */
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
    }
    /* On failure, leave cleanup to a UI boundary, not a potentially blocking
     * last-stream close inside moving-ball simulation. */
}
pool_sound_effect_t pool_sound_event(const pool_game_t* before, const pool_game_t* after)
{
    if (!before->rules.complete && after->rules.complete) {
        return POOL_SOUND_RESULT;
    }
    if (before->shot_set && !after->shot_set && after->rules.foul != POOL_FAIR) {
        return POOL_SOUND_FOUL;
    }
    if (after->shot_pots.count > before->shot_pots.count || (!before->rules.reracked && after->rules.reracked)) {
        return POOL_SOUND_POT;
    }
    if (!before->contacts.rail_after && after->contacts.rail_after) {
        return POOL_SOUND_RAIL;
    }
    if (before->contacts.first == 0U && after->contacts.first != 0U) {
        return POOL_SOUND_BALL;
    }
    if (!before->shot_set && after->shot_set) {
        return POOL_SOUND_CUE;
    }
    return POOL_SOUND_NONE;
}
