#include "camera_ccm.h"

#include <math.h>

bool esp32p4_camera_bound_ccm(float matrix[3][3])
{
    const float hardware_limit = 4.0f;
    const float target_limit   = 3.99f;
    float neutral[3]           = {0};
    bool outside               = false;
    for (unsigned int row = 0U; row < 3U; ++row) {
        for (unsigned int column = 0U; column < 3U; ++column) {
            const float value = matrix[row][column];
            if (!isfinite(value)) {
                return false;
            }
            outside       = outside || value < -hardware_limit || value > hardware_limit;
            neutral[row] += value;
        }
        // Do not silently alter neutral exposure if the diagonal baseline itself
        // cannot fit. Leave these cases to the driver's existing error handling.
        if (!isfinite(neutral[row]) || neutral[row] < -target_limit || neutral[row] > target_limit) {
            return false;
        }
    }
    if (!outside) {
        return false;
    }

    // Interpolate all coefficients by the same amount towards a diagonal matrix
    // with identical row sums. This reduces color correction without shifting
    // the white point. The small headroom avoids rounding onto a hardware limit.
    float amount = 1.0f;
    for (unsigned int row = 0U; row < 3U; ++row) {
        for (unsigned int column = 0U; column < 3U; ++column) {
            const float baseline = row == column ? neutral[row] : 0.0f;
            const float value    = matrix[row][column];
            float allowed        = 1.0f;
            if (value > target_limit) {
                allowed = (target_limit - baseline) / (value - baseline);
            } else if (value < -target_limit) {
                allowed = (-target_limit - baseline) / (value - baseline);
            }
            if (allowed < amount) {
                amount = allowed;
            }
        }
    }
    for (unsigned int row = 0U; row < 3U; ++row) {
        for (unsigned int column = 0U; column < 3U; ++column) {
            const float baseline = row == column ? neutral[row] : 0.0f;
            matrix[row][column]  = baseline + amount * (matrix[row][column] - baseline);
        }
    }
    return true;
}
