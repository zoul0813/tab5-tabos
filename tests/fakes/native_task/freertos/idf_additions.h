#pragma once
#include "task.h"
BaseType_t xTaskCreateWithCaps(void (*entry)(void*), const char* name, size_t stack_depth, void* argument,
                               UBaseType_t priority, TaskHandle_t* created, UBaseType_t capabilities);
void vTaskDeleteWithCaps(TaskHandle_t task);
