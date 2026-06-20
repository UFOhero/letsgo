OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    . = 0x80200000;
    .text : { *(.text) *(.text.*) }
    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) *(.data.*) *(.sdata) *(.sdata.*) }
    .bss : { *(.bss) *(.bss.*) *(.sbss) *(.sbss.*) *(COMMON) }
    . = ALIGN(4096);
    _end = .;
}