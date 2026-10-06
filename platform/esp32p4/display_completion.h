#ifndef TAB5_DISPLAY_COMPLETION_H
#define TAB5_DISPLAY_COMPLETION_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stdbool.h>

// A missing completion is fatal: DMA may still own borrowed application buffers.
bool tab5_display_wait_completion(SemaphoreHandle_t completion, const char* operation);

#endif
