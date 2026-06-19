.section .text
.globl _start
_start:
    li t0, 0x10000000
    li t1, 'S'
    sb t1, 0(t0)
    la sp, _stack_top
    call kernel_main
    wfi
    j .

.section .bss
.align 12
_stack_bottom:
    .space 4096
_stack_top: