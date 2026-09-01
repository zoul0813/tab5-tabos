#ifndef TABOS_SYSTEM_H
#define TABOS_SYSTEM_H

#include <stdint.h>

#define TABOS_APPLICATION_ABI_VERSION 3U

typedef struct {
        char target[16];
        char device[32];
        char display[32];
        uint32_t display_width;
        uint32_t display_height;
        uint32_t cpu_cores;
        uint32_t cpu_frequency_mhz;
        uint64_t memory_total_bytes;
        uint64_t external_memory_total_bytes;
} tabos_system_info_t;

int tabos_system_info(tabos_system_info_t* info);

#endif
