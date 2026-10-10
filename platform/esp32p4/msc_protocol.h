#ifndef TAB5_MSC_PROTOCOL_H
#define TAB5_MSC_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
        char line[16];
        size_t used;
        bool overflow;
} tab5_msc_command_t;

static inline bool tab5_msc_command_feed(tab5_msc_command_t* command, char byte)
{
    if (byte == '\r' || byte == '\n') {
        bool requested = !command->overflow && command->used == 9U && memcmp(command->line, "TABOS MSC", 9U) == 0;
        *command       = (tab5_msc_command_t) {0};
        return requested;
    }
    if (command->used < sizeof(command->line)) {
        command->line[command->used++] = byte;
    } else {
        command->overflow = true;
    }
    return false;
}

#define TAB5_MSC_BOOT_MAGIC UINT32_C(0x4d534331)
static inline bool tab5_msc_boot_consume(uint32_t* marker, bool software_reset)
{
    bool requested = software_reset && *marker == TAB5_MSC_BOOT_MAGIC;
    *marker        = 0U;
    return requested;
}

#endif
