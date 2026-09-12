#include <snake/sound.h>

#include <stdint.h>

enum {
    SAMPLE_RATE = 44100,
    NOTE_FRAMES = SAMPLE_RATE * 60 / 1000,
    MAX_FRAMES  = NOTE_FRAMES * 3
};

/* Static storage keeps the generated effect off the application's 16 KiB stack. */
static int16_t samples[MAX_FRAMES];

void snake_sound_close(snake_sound_t* sound)
{
    if (sound->stream != TABOS_AUDIO_STREAM_INVALID) {
        (void) tabos_audio_close(sound->stream);
        sound->stream = TABOS_AUDIO_STREAM_INVALID;
    }
}

void snake_sound_stop(snake_sound_t* sound)
{
    if (sound->stream != TABOS_AUDIO_STREAM_INVALID && tabos_audio_flush(sound->stream) != 0) {
        snake_sound_close(sound);
    }
}

void snake_sound_toggle(snake_sound_t* sound)
{
    sound->muted = !sound->muted;
    if (sound->muted) {
        snake_sound_close(sound);
    }
}

void snake_sound_play(snake_sound_t* sound, snake_sound_effect_t effect)
{
    static const uint16_t melodies[][3] = {
        {440U,  660U,  880U},
        {880U, 1320U,    0U},
        {330U,  220U,  110U},
        {660U,  880U, 1320U},
    };
    if (sound->muted || effect < SNAKE_SOUND_START || effect > SNAKE_SOUND_WIN) {
        return;
    }
    if (sound->stream == TABOS_AUDIO_STREAM_INVALID) {
        const tabos_audio_config_t config = {
            .direction   = TABOS_AUDIO_PLAYBACK,
            .channels    = 1U,
            .route       = TABOS_AUDIO_ROUTE_SPEAKER,
            .sample_rate = SAMPLE_RATE,
        };
        sound->stream = tabos_audio_open(&config);
        if (sound->stream == TABOS_AUDIO_STREAM_INVALID) {
            return; /* Audio is optional: gameplay continues if a device is unavailable. */
        }
    }
    snake_sound_stop(sound);
    if (sound->stream == TABOS_AUDIO_STREAM_INVALID) {
        return;
    }
    const unsigned int notes = effect == SNAKE_SOUND_EAT ? 2U : 3U;
    for (unsigned int note = 0U; note < notes; ++note) {
        uint32_t phase           = 0U;
        const uint32_t increment = (uint32_t) melodies[effect][note] * 65536U / SAMPLE_RATE;
        for (unsigned int i = 0U; i < NOTE_FRAMES; ++i) {
            /* Quiet triangle wave with a short attack and decay on every note. */
            phase                 = (phase + increment) & 65535U;
            int32_t wave          = phase < 32768U ? (int32_t) phase : 65535 - (int32_t) phase;
            wave                  = (wave - 16384) / 4;
            unsigned int envelope = i < 220U ? i : 220U;
            if (NOTE_FRAMES - 1U - i < envelope) {
                envelope = NOTE_FRAMES - 1U - i;
            }
            samples[note * NOTE_FRAMES + i] = (int16_t) (wave * (int32_t) envelope / 220);
        }
    }
    const uint32_t bytes = notes * NOTE_FRAMES * (uint32_t) sizeof(samples[0]);
    uint32_t written     = 0U;
    while (written < bytes) {
        const int result = tabos_audio_write(sound->stream, (const uint8_t*) samples + written, bytes - written);
        if (result <= 0) {
            /* Never wait for audio space on the game/input thread. */
            snake_sound_close(sound);
            return;
        }
        written += (uint32_t) result;
    }
}
