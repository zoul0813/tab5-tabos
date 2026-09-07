#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t StackType_t;
typedef uint32_t TickType_t;
typedef struct fake_native_task* TaskHandle_t;
#define pdPASS 1
#define pdFAIL 0
