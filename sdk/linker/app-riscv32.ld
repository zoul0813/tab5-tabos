ENTRY(tabos_elf_entry)

PHDRS
{
    load PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 0;
    .text : {
        *(.text.tabos_elf_entry)
        *(.text*)
        *(.rodata*)
    } :load
    .data : {
        *(.data*)
        *(.sdata*)
    } :load
    .bss (NOLOAD) : {
        *(.bss*)
        *(.sbss*)
        *(COMMON)
    } :load
    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
        *(.riscv.attributes*)
    }
}
