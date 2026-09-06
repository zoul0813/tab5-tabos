#include "camera_ccm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int check_correction(float matrix[3][3])
{
    float sums[3] = {0};
    for (unsigned int row = 0U; row < 3U; ++row) {
        for (unsigned int column = 0U; column < 3U; ++column) {
            sums[row] += matrix[row][column];
        }
    }
    if (!esp32p4_camera_bound_ccm(matrix)) {
        return 1;
    }
    for (unsigned int row = 0U; row < 3U; ++row) {
        float sum = 0.0f;
        for (unsigned int column = 0U; column < 3U; ++column) {
            if (!isfinite(matrix[row][column]) || fabsf(matrix[row][column]) > 4.0f) {
                return 1;
            }
            sum += matrix[row][column];
        }
        if (fabsf(sum - sums[row]) > 0.00001f) {
            return 1;
        }
    }
    return esp32p4_camera_bound_ccm(matrix) ? 1 : 0;
}

int main(void)
{
    // Exact final matrix captured from the physical Tab5 failure.
    float measured[3][3] = {
        { 2.950245f, -0.407000f, -1.588085f},
        {-0.727611f,  1.953100f, -0.613378f},
        {-0.340350f, -1.331600f,  4.318523f}
    };
    int failures         = check_correction(measured);
    float negative[3][3] = {
        {-4.6f, 2.8f, 2.8f},
        { 0.0f, 1.0f, 0.0f},
        { 0.0f, 0.0f, 1.0f}
    };
    failures += check_correction(negative);
    for (unsigned int index = 0U; index < 50U; ++index) {
        const float x      = 3.1f + (float) index * 0.1f;
        float matrix[3][3] = {
            {1.0f + x,       -x,     0.0f},
            {      -x, 1.0f + x,     0.0f},
            {    0.0f,       -x, 1.0f + x}
        };
        failures += check_correction(matrix);
    }
    float unchanged[][3][3] = {
        {    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {   {4.0f, -3.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {     {NAN, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{INFINITY, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {    {5.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };
    for (unsigned int index = 0U; index < sizeof(unchanged) / sizeof(unchanged[0]); ++index) {
        float original[3][3];
        memcpy(original, unchanged[index], sizeof(original));
        if (esp32p4_camera_bound_ccm(unchanged[index]) || memcmp(original, unchanged[index], sizeof(original)) != 0) {
            ++failures;
        }
    }
    if (failures != 0) {
        fprintf(stderr, "CCM bounds/neutral preservation failures: %d\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
