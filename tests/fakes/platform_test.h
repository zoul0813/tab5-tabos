#ifndef TABOS_TEST_PLATFORM_TEST_H
#define TABOS_TEST_PLATFORM_TEST_H

#include <stdint.h>

void tab_test_platform_set_time_ms(uint64_t time_ms);
void tab_test_platform_advance_time_ms(uint64_t elapsed_ms);

#endif
