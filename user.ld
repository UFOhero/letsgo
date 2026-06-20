OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS {
    . = 0x40000000;
    .text : { *(.text.entry) *(.text) *(.text.*) }
    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) *(.data.*) *(.sdata) *(.sdata.*) }
    .bss : { *(.bss) *(.bss.*) *(.sbss) *(.sbss.*) *(COMMON) }
}