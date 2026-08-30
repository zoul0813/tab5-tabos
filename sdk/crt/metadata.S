/* ELF SHT_NOTE resource metadata emitted for every SDK-built application. */

    .section .note.tabos,"",@note
    .balign 4
    .long 6                         /* name size: TABOS plus trailing NUL */
    .long 32                        /* descriptor size */
    .long 1                         /* TabOS metadata note type */
    .asciz "TABOS"
    .balign 4
    .long 1                         /* metadata version */
    .long 32                        /* descriptor size */
    .long TABOS_APP_ABI_VERSION
    .long TABOS_APP_HEAP_BYTES
    .long TABOS_APP_STACK_BYTES
    .long TABOS_APP_CAPABILITIES
    .long 0                         /* reserved */
    .long 0                         /* reserved */
