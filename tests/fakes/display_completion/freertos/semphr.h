#pragma once

#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout);
