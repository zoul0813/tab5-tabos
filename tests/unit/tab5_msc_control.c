#include "../../platform/esp32p4/msc_protocol.h"
#include <assert.h>
#include "../../platform/esp32p4/device_test_protocol.h"

static unsigned feed(tab5_msc_command_t* command, const char* bytes)
{
    unsigned requests = 0U;
    for (; *bytes != '\0'; ++bytes) {
        requests += tab5_msc_command_feed(command, *bytes) ? 1U : 0U;
    }
    return requests;
}

int main(void)
{
    tab5_test_input_t input;
    assert(tab5_test_parse_input("KEY 4 0 1", &input) && input.key == 4U && input.down == 1U);
    assert(tab5_test_parse_input("KEY 260 17 0", &input));
    assert(!tab5_test_parse_input("KEY 999 0 1", &input));
    assert(!tab5_test_parse_input("KEY 4 32 1", &input));
    assert(!tab5_test_parse_input("KEY 4 0 2", &input));
    assert(!tab5_test_parse_input("KEY 4 0 1 extra", &input));
    assert(!tab5_test_parse_input("KEY 99999999999999999999 0 1", &input));
    assert(tab5_test_parse_input("TEXT 68656c6c6f", &input) && strcmp(input.text, "hello") == 0);
    assert(!tab5_test_parse_input("TEXT 00", &input));
    assert(!tab5_test_parse_input("TEXT 0a", &input));
    assert(!tab5_test_parse_input("TEXT abx1", &input));
    assert(!tab5_test_parse_input("TEXT a", &input));
    assert(!tab5_test_parse_input("TEXT 61616161616161616161", &input));
    tab5_msc_command_t command = {0};
    assert(feed(&command, "TABOS ") == 0U);
    assert(feed(&command, "MSC\r\n") == 1U);
    assert(feed(&command, "noise TABOS MSC\nTABOS MSC extra\n") == 0U);
    assert(feed(&command, "xxxxxxxxxxxxxxxxxxxxxxxxTABOS MSC\n") == 0U);
    assert(feed(&command, "TABOS MSC\nTABOS MSC\n") == 2U);
    uint32_t marker = TAB5_MSC_BOOT_MAGIC;
    assert(tab5_msc_boot_consume(&marker, true));
    assert(!tab5_msc_boot_consume(&marker, true));
    marker = TAB5_MSC_BOOT_MAGIC;
    assert(!tab5_msc_boot_consume(&marker, false) && marker == 0U);
    marker = UINT32_MAX;
    assert(!tab5_msc_boot_consume(&marker, true) && marker == 0U);
    return 0;
}
