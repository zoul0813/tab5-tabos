#include "doom_tabos_sfx.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static int test_dmx_decode(void)
{
    uint8_t lump[64] = {0};
    lump[0]          = 0x03U;
    lump[2]          = 0x11U;
    lump[3]          = 0x2bU;
    lump[4]          = 56U;
    lump[24]         = 42U;
    const uint8_t* samples;
    size_t sample_count;
    uint32_t sample_rate;
    if (!doom_tabos_sfx_decode_dmx(lump, sizeof(lump), &samples, &sample_count, &sample_rate) ||
        samples != lump + 24U || sample_count != 24U || sample_rate != 11025U || samples[0] != 42U) {
        return 1;
    }
    lump[0] = 0U;
    return doom_tabos_sfx_decode_dmx(lump, sizeof(lump), &samples, &sample_count, &sample_rate) ? 1 : 0;
}

static int test_resampling_and_lifecycle(void)
{
    doom_tabos_sfx_mixer_t mixer;
    doom_tabos_sfx_init(&mixer);
    const uint8_t samples[] = {128U, 255U};
    if (!doom_tabos_sfx_start(&mixer, 0U, samples, 2U, 24000U, 127, 128)) {
        return 1;
    }
    int16_t output[10] = {0};
    doom_tabos_sfx_mix(&mixer, output, 5U);
    if (output[0] <= 0 || output[1] <= 0 || output[2] != output[0] || output[3] != output[1] ||
        output[4] <= output[2] || output[5] <= output[3] || output[6] < output[4] || output[7] < output[5] ||
        output[8] != 0 || output[9] != 0 || doom_tabos_sfx_playing(&mixer, 0U)) {
        doom_tabos_sfx_destroy(&mixer);
        return 1;
    }
    doom_tabos_sfx_destroy(&mixer);
    return 0;
}

static int test_pan_volume_and_stop(void)
{
    doom_tabos_sfx_mixer_t mixer;
    doom_tabos_sfx_init(&mixer);
    const uint8_t samples[] = {255U, 255U};
    if (!doom_tabos_sfx_start(&mixer, 2U, samples, 2U, DOOM_TABOS_SFX_SAMPLE_RATE, 127, 0)) {
        return 1;
    }
    int16_t output[2] = {0};
    doom_tabos_sfx_mix(&mixer, output, 1U);
    if (output[0] <= 0 || output[1] != 0) {
        doom_tabos_sfx_destroy(&mixer);
        return 1;
    }
    doom_tabos_sfx_set_params(&mixer, 2U, 64, 254);
    doom_tabos_sfx_mix(&mixer, output, 1U);
    if (output[0] != 0 || output[1] <= 0 || !doom_tabos_sfx_playing(&mixer, 2U)) {
        doom_tabos_sfx_destroy(&mixer);
        return 1;
    }
    doom_tabos_sfx_stop(&mixer, 2U);
    if (doom_tabos_sfx_playing(&mixer, 2U)) {
        doom_tabos_sfx_destroy(&mixer);
        return 1;
    }
    doom_tabos_sfx_destroy(&mixer);
    return 0;
}

static int test_resampling_low_pass_filter(void)
{
    doom_tabos_sfx_mixer_t mixer;
    doom_tabos_sfx_init(&mixer);
    const uint8_t samples[] = {255U, 0U};
    if (!doom_tabos_sfx_start(&mixer, 0U, samples, 2U, DOOM_TABOS_SFX_SAMPLE_RATE, 127, 128)) {
        return 1;
    }
    int16_t output[4] = {0};
    doom_tabos_sfx_mix(&mixer, output, 2U);
    const int result = output[0] > 0 && output[1] > 0 && output[2] < 0 && output[3] < 0 && output[2] > -output[0] &&
                               output[3] > -output[1] ?
                           0 :
                           1;
    doom_tabos_sfx_destroy(&mixer);
    return result;
}

static int test_saturating_mix(void)
{
    doom_tabos_sfx_mixer_t mixer;
    doom_tabos_sfx_init(&mixer);
    const uint8_t sample = 255U;
    for (unsigned int channel = 0U; channel < DOOM_TABOS_SFX_CHANNELS; ++channel) {
        if (!doom_tabos_sfx_start(&mixer, channel, &sample, 1U, DOOM_TABOS_SFX_SAMPLE_RATE, 127, 128)) {
            doom_tabos_sfx_destroy(&mixer);
            return 1;
        }
    }
    int16_t output[2] = {0};
    doom_tabos_sfx_mix(&mixer, output, 1U);
    const int result = output[0] == INT16_MAX && output[1] == INT16_MAX ? 0 : 1;
    doom_tabos_sfx_destroy(&mixer);
    return result;
}

static int test_independent_channel_render(void)
{
    doom_tabos_sfx_mixer_t mixer;
    doom_tabos_sfx_init(&mixer);
    const uint8_t positive[] = {255U, 255U};
    const uint8_t negative[] = {0U, 0U};
    if (!doom_tabos_sfx_start(&mixer, 0U, positive, 2U, DOOM_TABOS_SFX_SAMPLE_RATE, 127, 128) ||
        !doom_tabos_sfx_start(&mixer, 1U, negative, 2U, DOOM_TABOS_SFX_SAMPLE_RATE, 127, 128)) {
        doom_tabos_sfx_destroy(&mixer);
        return 1;
    }
    int16_t output[2] = {0};
    doom_tabos_sfx_render_channel(&mixer, 0U, output, 1U);
    const int result = output[0] > 0 && output[1] > 0 && mixer.channels[1].position == 0U ? 0 : 1;
    doom_tabos_sfx_destroy(&mixer);
    return result;
}

int main(void)
{
    if (test_dmx_decode() != 0) {
        (void) fprintf(stderr, "DMX decode failed\n");
        return 1;
    }
    if (test_resampling_and_lifecycle() != 0) {
        (void) fprintf(stderr, "resampling lifecycle failed\n");
        return 1;
    }
    if (test_pan_volume_and_stop() != 0) {
        (void) fprintf(stderr, "pan, volume, and stop failed\n");
        return 1;
    }
    if (test_resampling_low_pass_filter() != 0) {
        (void) fprintf(stderr, "resampling low-pass filter failed\n");
        return 1;
    }
    if (test_saturating_mix() != 0) {
        (void) fprintf(stderr, "saturating mix failed\n");
        return 1;
    }
    if (test_independent_channel_render() != 0) {
        (void) fprintf(stderr, "independent channel render failed\n");
        return 1;
    }
    return 0;
}
