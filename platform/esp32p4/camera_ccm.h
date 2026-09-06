#ifndef TABOS_ESP32P4_CAMERA_CCM_H
#define TABOS_ESP32P4_CAMERA_CCM_H

#include <stdbool.h>

// Bound an out-of-range final matrix while retaining each row sum (neutral RGB
// response). Return false without changes for valid or unsupported matrices.
bool esp32p4_camera_bound_ccm(float matrix[3][3]);

#endif
