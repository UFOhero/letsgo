// trap.c 完整修改后代码（仅展示修改后的 case 12/13/15 部分，其余不变）
// 在文件开头确保包含 proc.h
#include <stdint.h>
#include "syscall.h"
#include "csr.h"
#include "proc.h"
#include "pmm.h"
#include "vmm.h"
extern uint64_t *kernel_pagetable;
extern void timer_set_next(void);
extern int printf(const char *fmt, ...);
extern void trap_vector(void);
extern uint32_t plic_claim(void);
extern void plic_complete(uint32_t irq);
extern char uart_getc(void);

void trap_init(void) {
    w_stvec((uint64_t)trap_vector);
    printf("[Init] Trap vectors loaded at 0x%lx\n", (uint64_t)trap_vector);
}

void panic(const char *msg) {
    printf("PANIC: %s\n", msg);
    while (1) { __asm__ volatile("wfi"); }
}

void trap_handler(struct trapframe *tf) {
    uint64_t scause = r_scause();
    uint64_t is_interrupt = scause & (1ULL << 63);
    uint64_t cause_code = scause & 0x7FFFFFFFFFFFFFFF;

    if (is_interrupt) {
        switch (cause_code) {
            case 5:
                sched_tick();
                timer_set_next();
                break;
            case 9: {
                uint32_t irq = plic_claim();
                if (irq == 10) {
                    extern void uart_intr(void);
                    uart_intr();
                }
                if (irq) plic_complete(irq);
                break;
            }
            default:
                printf("Unknown Interrupt: %ld\n", cause_code);
                panic("Unhandled Interrupt");
        }
    } else {
        switch (cause_code) {
            case 2:
                printf("Exception: Illegal Instruction at 0x%lx\n", r_sepc());
                panic("Stop.");
                break;
            case 3:
                handle_syscall(tf);
                tf->sepc += 2;
                break;
            case 5:
            case 7:
                printf("  -> [Trap] Physical Memory Access Fault! Cause: %ld\n", cause_code);
                printf("  -> [Trap] Faulting Address (stval): 0x%lx\n", r_stval());
                printf("  -> [Trap] Faulting Instruction (sepc): 0x%lx\n", r_sepc());
                panic("Accessing invalid physical memory. System Halted.");
                break;
            case 8:
            case 9:
                handle_syscall(tf);
                tf->sepc += 4;
                break;
            case 12: // Instruction Page Fault
            case 13: // Load Page Fault
            case 15: // Store/AMO Page Fault
            {
                uint64_t fault_addr = r_stval();
                printf("\n  -> [Trap] Catch Page Fault! Cause: %ld, Faulting VA: 0x%lx\n", 
                    cause_code, fault_addr);

                uint64_t sstatus = r_sstatus();
                int from_user = !(sstatus & SSTATUS_SPP);
                printf("  -> [Trap] from_user=%d\n", from_user);

                uint64_t aligned_va = fault_addr & ~0xFFFULL;

                if (from_user) {
                    // 用户态缺页：直接为用户页表分配新页
                    void *page = pmm_alloc_frame();
                    if (!page) panic("OOM during Demand Paging!");
                    vmm_map_page(current->pagetable, aligned_va, (uint64_t)page,
                                PTE_R | PTE_W | PTE_X | PTE_U);
                } else {
                    // S-mode 缺页：可能是内核访问用户内存
                    // 先尝试到用户页表中找到已有的物理页
                    uint64_t pa = 0;
                    if (current && current->pagetable)
                        pa = vmm_lookup_page(current->pagetable, fault_addr);

                    if (pa != 0) {
                        printf("  -> [Trap] Reusing user page, pa=0x%lx\n", pa & ~0xFFFULL);
                        vmm_map_page(kernel_pagetable, aligned_va, pa & ~0xFFFULL,
                                    PTE_R | PTE_W);       // 内核权限，不带 U
                    } else {
                        // 找不到：分配新页（内核自身内存需求）
                        void *page = pmm_alloc_frame();
                        if (!page) panic("OOM during Demand Paging!");
                        vmm_map_page(kernel_pagetable, aligned_va, (uint64_t)page,
                                    PTE_R | PTE_W | PTE_X);
                    }
                }
                __asm__ volatile("sfence.vma zero, zero");
                break;
            }
            default:
                printf("Unknown Exception: %ld at 0x%lx\n", cause_code, r_sepc());
                panic("Unhandled Exception");
        }
    }
}