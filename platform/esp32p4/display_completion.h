#ifndef TAB5_DISPLAY_COMPLETION_H
#define TAB5_DISPLAY_COMPLETION_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stdbool.h>
#include <stdint.h>

// A missing completion is fatal: DMA may still own borrowed application buffers.
bool tab5_display_wait_completion(SemaphoreHandle_t completion, const char* operation);

/* Exactly one OS-owned back buffer can be pending scanout. It cannot become
 * writable until completion releases the previous front buffer. */
typedef struct {
        uint16_t* front;
        uint16_t* back;
        bool pending;
} tab5_display_scanout_t;
bool tab5_display_scanout_finish(tab5_display_scanout_t* scanout, SemaphoreHandle_t completion);

/* The producer publishes success before signaling completion. The owner must
 * finish before reusing any data consumed by that producer. */
typedef struct {
        bool pending;
        bool succeeded;
} tab5_display_work_t;
bool tab5_display_work_finish(tab5_display_work_t* work, SemaphoreHandle_t completion);

#endif
