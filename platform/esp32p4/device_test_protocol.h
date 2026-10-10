#ifndef TAB5_DEVICE_TEST_PROTOCOL_H
#define TAB5_DEVICE_TEST_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
        unsigned key;
        unsigned modifiers;
        unsigned down;
        char text[10];
} tab5_test_input_t;

static inline bool tab5_test_parse_input(const char* line, tab5_test_input_t* input)
{
    static const char digits[] = "0123456789abcdef";
    char extra;
    *input = (tab5_test_input_t) {0};
    /* Width limits prevent numeric scanf overflow on malformed serial input. */
    if (sscanf(line, "KEY %3u %2u %1u %c", &input->key, &input->modifiers, &input->down, &extra) == 3) {
        return ((input->key >= 4U && input->key <= 82U) || (input->key >= 256U && input->key <= 260U)) &&
               input->modifiers <= 31U && input->down <= 1U;
    }
    if (strncmp(line, "TEXT ", 5U) != 0) {
        return false;
    }
    const char* hex     = line + 5;
    const size_t length = strlen(hex);
    if (length == 0U || length > 18U || length % 2U != 0U) {
        return false;
    }
    for (size_t i = 0U; i < length; ++i) {
        const char* digit = strchr(digits, hex[i]);
        if (digit == NULL) {
            return false;
        }
        const unsigned nibble  = (unsigned) (digit - digits);
        input->text[i / 2U]   |= (char) (nibble << (i % 2U == 0U ? 4U : 0U));
    }
    for (size_t i = 0U; i < length / 2U; ++i) {
        if ((unsigned char) input->text[i] < 32U || (unsigned char) input->text[i] > 126U) {
            return false;
        }
    }
    return true;
}
#endif
