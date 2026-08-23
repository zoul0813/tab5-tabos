#include <tabos/config/acceleration.h>

#if TABOS_ENABLE_PIE
    .text
    .align 4
    .global esp32p4_pie_fill16_asm
    .type esp32p4_pie_fill16_asm, @function
esp32p4_pie_fill16_asm:
    esp.vldbc.16.ip q0, a2, 0
    srli a3, a1, 3
    beqz a3, 2f
1:
    esp.vst.128.ip q0, a0, 16
    addi a3, a3, -1
    bnez a3, 1b
2:
    andi a1, a1, 7
    lhu a2, 0(a2)
    beqz a1, 4f
3:
    sh a2, 0(a0)
    addi a0, a0, 2
    addi a1, a1, -1
    bnez a1, 3b
4:
    ret
    .size esp32p4_pie_fill16_asm, .-esp32p4_pie_fill16_asm

    .align 4
    .global esp32p4_pie_copy16_asm
    .type esp32p4_pie_copy16_asm, @function
esp32p4_pie_copy16_asm:
    srli a3, a2, 3
    beqz a3, 2f
1:
    esp.vld.128.ip q0, a1, 16
    esp.vst.128.ip q0, a0, 16
    addi a3, a3, -1
    bnez a3, 1b
2:
    andi a2, a2, 7
    beqz a2, 4f
3:
    lhu a3, 0(a1)
    sh a3, 0(a0)
    addi a0, a0, 2
    addi a1, a1, 2
    addi a2, a2, -1
    bnez a2, 3b
4:
    ret
    .size esp32p4_pie_copy16_asm, .-esp32p4_pie_copy16_asm
#endif
