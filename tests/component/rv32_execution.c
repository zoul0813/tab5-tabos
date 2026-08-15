#include <tabos/elf_api.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/platform/platform.h>

#include "hello_elf.h"

#include <stdbool.h>
#include <string.h>

static bool wrote_expected_message;
static bool requested_exit;
static int requested_status = -1;

static void test_console_write(const char *text)
{
    wrote_expected_message = text != NULL &&
        strcmp(text, "Hello from independent TabOS ELF") == 0;
}

static void test_request_exit(int status)
{
    requested_exit = true;
    requested_status = status;
}

static tab_platform_riscv32_result_t execute_raw(
    const uint32_t *instructions,
    size_t size,
    unsigned int budget)
{
    const tabos_elf_api_t api = {
        .abi_version = TABOS_ELF_API_VERSION,
        .console_write = test_console_write,
        .request_exit = test_request_exit,
    };
    int returned_status = -1;
    tab_platform_riscv32_context_t *context = tab_platform_riscv32_create(
        instructions, instructions, size, 0U, &api);
    if (context == NULL) return TAB_PLATFORM_RISCV32_FAULT;
    const tab_platform_riscv32_result_t result =
        tab_platform_riscv32_step(context, budget, &returned_status);
    tab_platform_riscv32_destroy(context);
    return result;
}

int main(void)
{
    tab_elf_image_t image;
    if (tab_elf_load(tab_hello_elf, tab_hello_elf_size, &image) != TAB_ELF_OK) return 1;

    const tabos_elf_api_t api = {
        .abi_version = TABOS_ELF_API_VERSION,
        .console_write = test_console_write,
        .request_exit = test_request_exit,
    };
    int returned_status = -1;
    tab_platform_riscv32_context_t *context = tab_platform_riscv32_create(
        image.entry, image.memory, image.memory_size, image.info.minimum_address, &api);
    tab_platform_riscv32_result_t result = TAB_PLATFORM_RISCV32_FAULT;
    if (context != NULL) {
        result = tab_platform_riscv32_step(context, 10000U, &returned_status);
    }
    tab_platform_riscv32_destroy(context);
    tab_elf_unload(&image);

    if (result != TAB_PLATFORM_RISCV32_RETURNED || !wrote_expected_message || !requested_exit ||
        requested_status != 0 || returned_status != 0) {
        return 1;
    }

    static const uint32_t illegal_instruction[] = {UINT32_C(0xffffffff)};
    static const uint32_t invalid_load[] = {
        UINT32_C(0x00200537), /* lui a0, 0x200: address 0x00200000 */
        UINT32_C(0x00052503), /* lw a0, 0(a0): beyond 1 MiB guest RAM */
    };
    static const uint32_t runaway_loop[] = {
        UINT32_C(0x0000006f), /* jal zero, 0 */
    };
    static const uint32_t resumable_program[] = {
        UINT32_C(0x00000013), /* nop */
        UINT32_C(0x00000013), /* nop */
        UINT32_C(0x00008067), /* ret */
    };
    tab_platform_riscv32_context_t *resumable = tab_platform_riscv32_create(
        resumable_program, resumable_program, sizeof(resumable_program), 0U, &api);
    int resumable_status = -1;
    const bool resumed = resumable != NULL &&
        tab_platform_riscv32_step(resumable, 2U, &resumable_status) ==
            TAB_PLATFORM_RISCV32_YIELDED &&
        tab_platform_riscv32_step(resumable, 2U, &resumable_status) ==
            TAB_PLATFORM_RISCV32_RETURNED;
    tab_platform_riscv32_destroy(resumable);

    return resumed &&
        execute_raw(illegal_instruction, sizeof(illegal_instruction), 1U) ==
            TAB_PLATFORM_RISCV32_FAULT &&
        execute_raw(invalid_load, sizeof(invalid_load), 2U) ==
            TAB_PLATFORM_RISCV32_FAULT &&
        execute_raw(runaway_loop, sizeof(runaway_loop), 1000U) ==
            TAB_PLATFORM_RISCV32_YIELDED ? 0 : 1;
}
