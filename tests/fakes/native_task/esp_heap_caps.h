#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM   1U
#define MALLOC_CAP_8BIT     2U
#define MALLOC_CAP_INTERNAL 4U
size_t heap_caps_get_free_size(unsigned int capabilities);
