.global trap_vector
.align 4

.equ SSTATUS_SPP, 256

trap_vector:

    csrrw sp, sscratch, sp
    bnez sp, 1f
    csrrw sp, sscratch, sp
    j 2f

1:
    addi sp, sp, -272
    sd ra,   0*8(sp)
    sd gp,   2*8(sp)
    sd tp,   3*8(sp)
    sd t0,   4*8(sp)
    csrr t0, sscratch
    sd t0,   1*8(sp)
    csrw sscratch, zero
    j 3f

2:
    addi sp, sp, -272
    sd ra,   0*8(sp)
    sd gp,   2*8(sp)
    sd tp,   3*8(sp)
    sd t0,   4*8(sp)
    addi t0, sp, 272
    sd t0,   1*8(sp)

3:
    sd t1,   5*8(sp)
    sd t2,   6*8(sp)
    sd s0,   7*8(sp)
    sd s1,   8*8(sp)
    sd a0,   9*8(sp)
    sd a1,  10*8(sp)
    sd a2,  11*8(sp)
    sd a3,  12*8(sp)
    sd a4,  13*8(sp)
    sd a5,  14*8(sp)
    sd a6,  15*8(sp)
    sd a7,  16*8(sp)
    sd s2,  17*8(sp)
    sd s3,  18*8(sp)
    sd s4,  19*8(sp)
    sd s5,  20*8(sp)
    sd s6,  21*8(sp)
    sd s7,  22*8(sp)
    sd s8,  23*8(sp)
    sd s9,  24*8(sp)
    sd s10, 25*8(sp)
    sd s11, 26*8(sp)
    sd t3,  27*8(sp)
    sd t4,  28*8(sp)
    sd t5,  29*8(sp)
    sd t6,  30*8(sp)
    csrr t0, sepc
    sd t0, 31*8(sp)
    csrr t0, sstatus
    sd t0, 32*8(sp)

    mv a0, sp
    call trap_handler

.global trap_return
trap_return:
    ld t0, 31*8(sp)
    csrw sepc, t0
    ld t0, 32*8(sp)
    csrw sstatus, t0

    andi t1, t0, SSTATUS_SPP
    bnez t1, 4f
    addi t1, sp, 272
    csrw sscratch, t1
    j 5f
4:
    csrw sscratch, zero

5:

    la t0, switch_to_user
    lw t1, 0(t0)
    beqz t1, 6f
    la t0, user_satp
    ld t1, 0(t0)
    csrw satp, t1
    sfence.vma zero, zero
    la t0, switch_to_user
    sw zero, 0(t0)

6:
    ld ra,   0*8(sp)
    ld gp,   2*8(sp)
    ld tp,   3*8(sp)
    ld t0,   4*8(sp)
    ld t1,   5*8(sp)
    ld t2,   6*8(sp)
    ld s0,   7*8(sp)
    ld s1,   8*8(sp)
    ld a0,   9*8(sp)
    ld a1,  10*8(sp)
    ld a2,  11*8(sp)
    ld a3,  12*8(sp)
    ld a4,  13*8(sp)
    ld a5,  14*8(sp)
    ld a6,  15*8(sp)
    ld a7,  16*8(sp)
    ld s2,  17*8(sp)
    ld s3,  18*8(sp)
    ld s4,  19*8(sp)
    ld s5,  20*8(sp)
    ld s6,  21*8(sp)
    ld s7,  22*8(sp)
    ld s8,  23*8(sp)
    ld s9,  24*8(sp)
    ld s10, 25*8(sp)
    ld s11, 26*8(sp)
    ld t3,  27*8(sp)
    ld t4,  28*8(sp)
    ld t5,  29*8(sp)
    ld t6,  30*8(sp)
    ld sp,   1*8(sp)
    sret
