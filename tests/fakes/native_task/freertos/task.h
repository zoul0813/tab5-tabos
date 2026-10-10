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
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskSuspend(TaskHandle_t task);
void vTaskResume(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);
eTaskState eTaskGetState(TaskHandle_t task);
void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index, void* value);
void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index);
