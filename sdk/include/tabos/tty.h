#ifndef TABOS_TTY_H
#define TABOS_TTY_H

#include <stdint.h>

enum {
    TABOS_TTY_MODE_SCROLL_KEYS = 1U << 0U,
    TABOS_TTY_MODE_RAW_INPUT   = 1U << 1U,
};

enum {
    TABOS_TTY_GET_MODE = 0x5401,
    TABOS_TTY_SET_MODE = 0x5402,
    TABOS_TTY_GET_SIZE = 0x5403,
};

typedef struct {
        uint32_t rows;
        uint32_t columns;
} tabos_tty_size_t;

#endif
