#ifndef TABOS_TEST_PLATFORM_TEST_H
#define TABOS_TEST_PLATFORM_TEST_H

#include <stdint.h>

void test_platform_set_time_ms(uint64_t time_ms);
void test_platform_advance_time_ms(uint64_t elapsed_ms);
const char *test_storage_root(void);
const char *test_platform_last_log(void);
void test_platform_clear_log(void);

#endif
