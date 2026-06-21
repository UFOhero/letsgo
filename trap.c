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

volatile uint64_t trap_timer_count = 0;
volatile uint64_t trap_uart_count = 0;
volatile uint64_t trap_syscall_count = 0;
volatile uint64_t trap_page_fault_count = 0;

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
                trap_timer_count++;
                sched_tick();
                timer_set_next();
                break;
            case 9: {
                uint32_t irq = plic_claim();
                if (irq == 10) {
                    trap_uart_count++;
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
            {
                trap_syscall_count++;
                uint64_t old_sepc = tf->sepc;
                handle_syscall(tf);
                if (tf->sepc == old_sepc) tf->sepc += 2;
                break;
            }
            case 5:
            case 7:
                printf("  -> [Trap] Physical Memory Access Fault! Cause: %ld\n", cause_code);
                printf("  -> [Trap] Faulting Address (stval): 0x%lx\n", r_stval());
                printf("  -> [Trap] Faulting Instruction (sepc): 0x%lx\n", r_sepc());
                panic("Accessing invalid physical memory. System Halted.");
                break;
            case 8:
            case 9:
            {
                trap_syscall_count++;
                uint64_t old_sepc = tf->sepc;
                handle_syscall(tf);
                if (tf->sepc == old_sepc) tf->sepc += 4;
                break;
            }
            case 12: // Instruction Page Fault
            case 13: // Load Page Fault
            case 15: // Store/AMO Page Fault
            {
                trap_page_fault_count++;
                uint64_t fault_addr = r_stval();
                uint64_t sstatus = r_sstatus();
                int from_user = !(sstatus & SSTATUS_SPP);
                uint64_t aligned_va = fault_addr & ~0xFFFULL;

                if (fault_addr < 0x80000000ULL) {
                    if (!current || !current->pagetable) {
                        panic("Page fault on user address, but no active process!");
                    }

                    // 检查用户页表中是否已经有了映射
                    if (fault_addr < PAGE_SIZE) {
                        printf("Fatal User Null Page Fault! VA: 0x%lx, cause: %ld\n", fault_addr, cause_code);
                        current->exit_code = -1;
                        extern void exit(void);
                        exit();
                        break;
                    }

                    uint64_t pa = vmm_lookup_page(current->pagetable, aligned_va);

                    if (pa == 0) {
                        // 真正的按需分配 (Demand Paging)
                        void *page = pmm_alloc_frame();
                        if (!page) panic("OOM during User Space Demand Paging!");
                        
                        // 这样用户进程，以及开启了 SUM 位的内核态，才能成功访问该页。
                        vmm_map_page(current->pagetable, aligned_va, (uint64_t)page, 
                                     PTE_R | PTE_W | PTE_X | PTE_U);
                    } else {
                        // 地址已映射却依然缺页，说明发生了权限越界（比如写了只读代码段）
                        printf("Fatal Protection Fault! VA: 0x%lx, cause: %ld\n", fault_addr, cause_code);
                        if (!from_user) panic("Kernel attempted to write read-only User Page!");
                        // 强制杀死越界的用户进程
                        current->exit_code = -1;
                        extern void exit(void);
                        exit();
                    }
                } else {
                    // 内核空间的缺页 (>= 0x80000000)
                    // 在直接映射的简易内核中不该发生，直接 Panic 保护系统
                    printf("Fatal S-Mode Page Fault! VA: 0x%lx, from_user=%d\n", fault_addr, from_user);
                    panic("Kernel Page Fault on Kernel Space Address!");
                }
                
                // 强制刷新 TLB，确保新的映射立马生效，防止 CPU 继续死循环
                __asm__ volatile("sfence.vma zero, zero");
                break;
            }
            default:
                printf("Unknown Exception: %ld at 0x%lx\n", cause_code, r_sepc());
                panic("Unhandled Exception");
        }
    }
}
