#include "doom_tabos_sfx.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    SFX_POSITION_BITS = 16,
    SFX_POSITION_MASK = (1U << SFX_POSITION_BITS) - 1U,
    SFX_GAIN_SHIFT = 8,
};

static int clamp(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int16_t saturate(int32_t sample)
{
    if (sample > INT16_MAX) {
        return INT16_MAX;
    }
    if (sample < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) sample;
}

void doom_tabos_sfx_init(doom_tabos_sfx_mixer_t* mixer)
{
    if (mixer != NULL) {
        memset(mixer, 0, sizeof(*mixer));
    }
}

void doom_tabos_sfx_stop(doom_tabos_sfx_mixer_t* mixer, unsigned int channel)
{
    if (mixer == NULL || channel >= DOOM_TABOS_SFX_CHANNELS) {
        return;
    }
    free(mixer->channels[channel].samples);
    mixer->channels[channel] = (doom_tabos_sfx_channel_t) {0};
}

void doom_tabos_sfx_destroy(doom_tabos_sfx_mixer_t* mixer)
{
    if (mixer == NULL) {
        return;
    }
    for (unsigned int channel = 0U; channel < DOOM_TABOS_SFX_CHANNELS; ++channel) {
        doom_tabos_sfx_stop(mixer, channel);
    }
}

bool doom_tabos_sfx_decode_dmx(const uint8_t* lump, size_t lump_size, const uint8_t** samples, size_t* sample_count,
                               uint32_t* sample_rate)
{
    if (lump == NULL || samples == NULL || sample_count == NULL || sample_rate == NULL || lump_size < 8U ||
        lump[0] != 0x03U || lump[1] != 0x00U) {
        return false;
    }
    const uint32_t rate = (uint32_t) lump[2] | ((uint32_t) lump[3] << 8U);
    const uint32_t length =
        (uint32_t) lump[4] | ((uint32_t) lump[5] << 8U) | ((uint32_t) lump[6] << 16U) | ((uint32_t) lump[7] << 24U);
    if (rate == 0U || length > lump_size - 8U || length <= 48U) {
        return false;
    }
    *samples      = lump + 24U;
    *sample_count = length - 32U;
    *sample_rate  = rate;
    return true;
}

void doom_tabos_sfx_set_params(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, int volume, int separation)
{
    if (mixer == NULL || channel >= DOOM_TABOS_SFX_CHANNELS) {
        return;
    }
    volume                              = clamp(volume, 0, 127);
    separation                          = clamp(separation, 0, 254);
    mixer->channels[channel].left_gain  = (uint16_t) (((254 - separation) * volume) / 127);
    mixer->channels[channel].right_gain = (uint16_t) ((separation * volume) / 127);
}

bool doom_tabos_sfx_start(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, const uint8_t* samples,
                          size_t sample_count, uint32_t sample_rate, int volume, int separation)
{
    if (mixer == NULL || channel >= DOOM_TABOS_SFX_CHANNELS || samples == NULL || sample_count == 0U ||
        sample_rate == 0U) {
        return false;
    }
    doom_tabos_sfx_stop(mixer, channel);
    uint8_t* copy = malloc(sample_count);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, samples, sample_count);
    uint64_t step = ((uint64_t) sample_rate << SFX_POSITION_BITS) / DOOM_TABOS_SFX_SAMPLE_RATE;
    if (step == 0U || step > UINT32_MAX) {
        free(copy);
        return false;
    }
    mixer->channels[channel] = (doom_tabos_sfx_channel_t) {
        .samples      = copy,
        .sample_count = sample_count,
        .step         = (uint32_t) step,
        .active       = true,
    };
    doom_tabos_sfx_set_params(mixer, channel, volume, separation);
    return true;
}

bool doom_tabos_sfx_playing(const doom_tabos_sfx_mixer_t* mixer, unsigned int channel)
{
    return mixer != NULL && channel < DOOM_TABOS_SFX_CHANNELS && mixer->channels[channel].active;
}

static bool mix_channel_frame(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, int32_t* left, int32_t* right)
{
    doom_tabos_sfx_channel_t* sound = &mixer->channels[channel];
    if (!sound->active) {
        return false;
    }
    if (sound->source_index >= sound->sample_count) {
        doom_tabos_sfx_stop(mixer, channel);
        return false;
    }
    const int32_t sample = ((int32_t) sound->samples[sound->source_index] - 128) * 256;
    *left               += (sample * sound->left_gain) >> SFX_GAIN_SHIFT;
    *right              += (sample * sound->right_gain) >> SFX_GAIN_SHIFT;
    sound->fraction     += sound->step;
    sound->source_index += sound->fraction >> SFX_POSITION_BITS;
    sound->fraction     &= SFX_POSITION_MASK;
    return true;
}

void doom_tabos_sfx_render_channel(doom_tabos_sfx_mixer_t* mixer, unsigned int channel, int16_t* stereo, size_t frames)
{
    if (mixer == NULL || channel >= DOOM_TABOS_SFX_CHANNELS || stereo == NULL) {
        return;
    }
    memset(stereo, 0, frames * 2U * sizeof(*stereo));
    for (size_t frame = 0U; frame < frames; ++frame) {
        int32_t left  = 0;
        int32_t right = 0;
        (void) mix_channel_frame(mixer, channel, &left, &right);
        stereo[frame * 2U]      = saturate(left);
        stereo[frame * 2U + 1U] = saturate(right);
    }
}

void doom_tabos_sfx_mix(doom_tabos_sfx_mixer_t* mixer, int16_t* stereo, size_t frames)
{
    if (mixer == NULL || stereo == NULL) {
        return;
    }
    memset(stereo, 0, frames * 2U * sizeof(*stereo));
    for (size_t frame = 0U; frame < frames; ++frame) {
        int32_t left  = 0;
        int32_t right = 0;
        for (unsigned int channel = 0U; channel < DOOM_TABOS_SFX_CHANNELS; ++channel) {
            (void) mix_channel_frame(mixer, channel, &left, &right);
        }
        stereo[frame * 2U]      = saturate(left);
        stereo[frame * 2U + 1U] = saturate(right);
    }
}
