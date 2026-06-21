.section .text
.globl swtch

# void swtch(uint64 *old_context, uint64 *new_context);
# 传入参数：a0 = old_context_ptr, a1 = new_context_ptr
swtch:
    # 1. 保存当前进程的上下文到 prev->context
    # 按照 proc.h 中的 uint64 context[32] 布局
    sd ra,  0*8(a0)
    sd sp,  1*8(a0)
    sd s0,  2*8(a0)
    sd s1,  3*8(a0)
    sd s2,  4*8(a0)
    sd s3,  5*8(a0)
    sd s4,  6*8(a0)
    sd s5,  7*8(a0)
    sd s6,  8*8(a0)
    sd s7,  9*8(a0)
    sd s8, 10*8(a0)
    sd s9, 11*8(a0)
    sd s10, 12*8(a0)
    sd s11, 13*8(a0)

    # 2. 从 next->context 恢复下一个进程的上下文
    ld ra,  0*8(a1)
    ld sp,  1*8(a1)
    ld s0,  2*8(a1)
    ld s1,  3*8(a1)
    ld s2,  4*8(a1)
    ld s3,  5*8(a1)
    ld s4,  6*8(a1)
    ld s5,  7*8(a1)
    ld s6,  8*8(a1)
    ld s7,  9*8(a1)
    ld s8, 10*8(a1)
    ld s9, 11*8(a1)
    ld s10, 12*8(a1)
    ld s11, 13*8(a1)


    # 在跳转前最后一刻，强行改写 a0 寄存器，传递返回值
    ld a0,  14*8(a1)

    # PC 跳转到被恢复的 ra 地址，sp 也已经变成了新栈
    ret