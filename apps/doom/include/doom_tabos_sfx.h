#ifndef DOOM_TABOS_SFX_H
#define DOOM_TABOS_SFX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    DOOM_TABOS_SFX_CHANNELS    = 8,
    DOOM_TABOS_SFX_SAMPLE_RATE = 11025,
};

typedef struct {
        uint8_t* samples;
        size_t sample_count;
        size_t source_index;
        uint32_t fraction;
        uint32_t step;
        uint16_t left_gain;
        uint16_t right_gain;
        bool active;
} doom_tabos_sfx_channel_t;

typedef struct {
        doom_tabos_sfx_channel_t channels[DOOM_TABOS_SFX_CHANNELS];
} doom_tabos_sfx_mixer_t;

void doom_tabos_sfx_init(doom_tabos_sfx_mixer_t* mixer);
void doom_tabos_sfx_destroy(doom_tabos_sfx_mixer_t* mixer);
bool doom_tabos_sfx_decode_dmx(const uint8_t* lump, size_t lump_size, const uint8_t** samples, size_t* sample_count,
                               uint32_t* sample_rate);
bool doom_tabos_sfx_start(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, const uint8_t* samples,
                          size_t sample_count, uint32_t sample_rate, int volume, int separation);
void doom_tabos_sfx_stop(doom_tabos_sfx_mixer_t* mixer, unsigned int channel);
bool doom_tabos_sfx_playing(const doom_tabos_sfx_mixer_t* mixer, unsigned int channel);
void doom_tabos_sfx_set_params(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, int volume, int separation);
void doom_tabos_sfx_render_channel(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, int16_t* stereo, size_t frames);
void doom_tabos_sfx_mix(doom_tabos_sfx_mixer_t* mixer, int16_t* stereo, size_t frames);

#endif
