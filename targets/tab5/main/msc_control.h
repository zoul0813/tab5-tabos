#ifndef TAB5_MSC_CONTROL_H
#define TAB5_MSC_CONTROL_H
#include <stdbool.h>
bool tab5_msc_control_start(void);
void tab5_msc_control_stop(void);
bool tab5_msc_control_take_request(void);
#ifdef TABOS_ENABLE_DEVICE_TEST_CONTROL
void tab5_test_control_update(void);
#endif
#endif
