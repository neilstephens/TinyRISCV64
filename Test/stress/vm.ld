ENTRY(main)

SECTIONS
{
    . = 0x0;

    .text   : { *(.text.init) *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }

    . = ALIGN(8);
    _end = .;

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.riscv.attributes) }
}


