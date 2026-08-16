#include <tester/test.h>

#include <stdint.h>
#include <stdlib.h>

void tester_test_heap(tester_context_t *context)
{
    uint8_t *bytes = malloc(1024U);
    tester_expect(context, bytes != NULL, "malloc allocates memory");
    if (bytes != NULL) {
        for (size_t index = 0; index < 1024U; ++index) bytes[index] = (uint8_t)index;
        uint8_t *grown = realloc(bytes, 2048U);
        tester_expect(context, grown != NULL, "realloc grows allocation");
        if (grown != NULL) {
            bool retained = true;
            for (size_t index = 0; index < 1024U; ++index) {
                if (grown[index] != (uint8_t)index) retained = false;
            }
            tester_expect(context, retained, "realloc preserves existing bytes");
            bytes = grown;
        }
        free(bytes);
    }

    uint32_t *zeroed = calloc(64U, sizeof(*zeroed));
    tester_expect(context, zeroed != NULL, "calloc allocates memory");
    if (zeroed != NULL) {
        bool all_zero = true;
        for (size_t index = 0; index < 64U; ++index) {
            if (zeroed[index] != 0U) all_zero = false;
        }
        tester_expect(context, all_zero, "calloc clears memory");
        free(zeroed);
    }
}
