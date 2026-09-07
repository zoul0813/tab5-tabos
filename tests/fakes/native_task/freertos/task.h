#pragma once
#include "FreeRTOS.h"
typedef enum {
    eRunning,
    eReady,
    eBlocked,
    eSuspended,
    eDeleted,
    eInvalid
} eTaskState;
void vTaskSuspend(TaskHandle_t task);
void vTaskResume(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
eTaskState eTaskGetState(TaskHandle_t task);
void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index, void* value);
void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index);
