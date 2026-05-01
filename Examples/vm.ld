ENTRY(main)

SECTIONS
{
    . = 0x0;

    .text   : { *(.text.init) *(.text*) }  /* Code                */
    .rodata : { *(.rodata*) }              /* Read-only data      */
    .data   : { *(.data*) }                /* Pre-nitialised data */
    .bss    : { *(.bss*) *(COMMON) }       /* Zeroed data         */

    . = ALIGN(8);
    
    /* Zero the heap size and malloc will return NULL */
    /* Comment this whole block and malloc won't compile */
    .heap (NOLOAD) : {
        __heap_start = .;
        . += 0x10000;         /* 64KB heap */
        __heap_end = .;
    }

    /* We would have to resurect eh_frame if we wanted to use C++ */
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.riscv.attributes) }
}


