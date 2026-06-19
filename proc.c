#include <stdint.h>
#include "proc.h"
#include "heap.h"
#include "csr.h"
#include "pmm.h"
#include "vmm.h"
#include "elf.h"
#include "fs.h"
#include "string.h"

#ifndef PTE_V
#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_U (1 << 4)
#endif

volatile int exit_code_trampoline;
extern uint64_t *kernel_pagetable;

void schedule(struct trapframe *tf) { (void)tf; }

struct proc procs[NPROC];
struct proc *current = 0;
static char stacks[NPROC][8192] __attribute__((aligned(16)));
static int next_pid = 0;
static uint64 next_ctime = 0;
static uint64 enq_clock = 0;
int sched_algorithm = 2;
volatile int need_resched = 0;

static struct proc* allocproc(void) {
    for (int i = 0; i < NPROC; i++) {
        if (procs[i].state == UNUSED) {
            memset(&procs[i], 0, sizeof(struct proc));
            procs[i].pid = i;
            procs[i].state = RUNNABLE;
            procs[i].kstack = (uint64)&stacks[i][8192] & ~0xf;
            procs[i].ctime = next_ctime++;
            procs[i].enq_time = enq_clock++;
            procs[i].slice = (sched_algorithm == 2) ? 1 : TIME_SLICE_COUNT;
            procs[i].priority = 0;
            procs[i].parent = current;
            procs[i].exit_code = 0;
            procs[i].chan = 0;
            procs[i].pagetable = 0;
            procs[i].ustack = 0;
            procs[i].entry = 0;
            return &procs[i];
        }
    }
    printf("allocproc: no free proc!\n");
    return 0;
}

int create_proc(void (*func)(void)) {
    struct proc *p = allocproc();
    if (!p) return -1;
    for (int i = 0; i < 32; i++) p->context[i] = 0;
    p->context[0] = (uint64)func;
    p->context[1] = p->kstack;
    return p->pid;
}

static struct proc* RR_select(void) {
    for (int i = 0; i < NPROC; i++) {
        int pid = (next_pid + i) % NPROC;
        if (procs[pid].state == RUNNABLE && &procs[pid] != current) {
            next_pid = (pid + 1) % NPROC;
            return &procs[pid];
        }
    }
    if (current && current->state == RUNNABLE) return current;
    return 0;
}

static struct proc* FCFS_select(void) {
    struct proc* selected = 0;
    uint64 min_time = ~0ULL;
    for (int i = 0; i < NPROC; i++) {
        if (procs[i].state == RUNNABLE) {
            if (procs[i].enq_time < min_time) {
                min_time = procs[i].enq_time;
                selected = &procs[i];
            }    
        }
    }
    if (selected == 0 && current && current->state == RUNNABLE) selected = current;
    return selected;
}

static struct proc* MLFQ_select(void) {
    for(int q = 0; q < NQUEUE; q++) {
        for(int i = 0; i < NPROC; i++) {
            if(procs[i].state == RUNNABLE && procs[i].priority == q)
                return &procs[i];
        }
    }
    if(current && current->state == RUNNABLE) return current;
    return 0;
}

struct proc* scheduler(void) {
    switch(sched_algorithm) {
        case 0: return RR_select();
        case 1: return FCFS_select();
        case 2: return MLFQ_select();
        default: return 0;
    }       
}

void yield(void) {
    struct proc *next = scheduler();
    if (next == 0 || next == current) return;
    struct proc *prev = current;
    current = next;
    if (prev && prev->state == RUNNING) {
        prev->state = RUNNABLE;
        prev->enq_time = enq_clock++;
        if(sched_algorithm == 2 && !need_resched) {
            if(prev->priority > 0) prev->priority--;
            prev->slice = 1;
        }
    }
    current->state = RUNNING;
    if(sched_algorithm != 2) current->slice = 10;
    swtch(prev->context, current->context);
}

void sched_tick(void) {
    if (current == 0) return;
    if (current->slice > 0) {
        current->slice--;
        if (current->slice == 0) {
            need_resched = 1;
            if(sched_algorithm == 2) {
                if(current->priority < NQUEUE - 1) current->priority++;
                current->slice = (1 << current->priority);
            }
        }
    }
}

void blocked(void *chan) {
    if (current == 0) return;
    current->chan = chan;
    current->state = BLOCKED;
    yield();
}

void wakeup(void *chan) {
    for (int i = 0; i < NPROC; i++) {
        if (procs[i].state == BLOCKED && procs[i].chan == chan) {
            procs[i].state = RUNNABLE;
            procs[i].chan = 0;
            procs[i].enq_time = enq_clock++;
        }
    }
}

void exit(void) {
    if (current == 0) return;
    __sync_synchronize();
    current->state = ZOMBIE;
    __sync_synchronize();
    wakeup(current->parent);
    for (int i = 0; i < NPROC; i++) {
        if (procs[i].parent == current) procs[i].parent = 0;
    }
    yield();
    while(1);
}

int wait(int pid, int *status) {
    if (current == 0) return -1;
    for (;;) {
        int have_kids = 0;
        for (int i = 0; i < NPROC; i++) {
            if (procs[i].parent != current) continue;
            if (pid != -1 && procs[i].pid != (uint64)pid) continue;
            have_kids = 1;
            if (procs[i].state == ZOMBIE) {
                int ret_pid = procs[i].pid;
                if (status) {
                    // 【核心防御】：防止上下文切换导致的 SUM 位丢失！
                    // 在对用户内存 (*status) 进行写操作的瞬间，强制重新开启 SUM 位。
                    uint64_t old_sstatus = r_sstatus();
                    w_sstatus(old_sstatus | (1ULL << 18));
                    *status = procs[i].exit_code;
                    w_sstatus(old_sstatus);
                }
                procs[i].state = UNUSED;
                return ret_pid;
            }
        }
        if (!have_kids) return -1;
        blocked(current);
    }
}

// -------------------------------------------------------------
// 【深度克隆用户页表】 物理隔离父子进程
// -------------------------------------------------------------
uint64_t* clone_user_pagetable(uint64_t *parent_pt) {
    uint64_t *child_pt = (uint64_t *)pmm_alloc_frame();
    if (!child_pt) return 0;
    memset(child_pt, 0, 4096); 
    
    for(int i = 256; i < 512; i++) child_pt[i] = parent_pt[i];
    for(int i = 2; i < 256; i++) child_pt[i] = parent_pt[i];

    for (int i = 0; i < 2; i++) {
        uint64_t pte1 = parent_pt[i];
        if ((pte1 & PTE_V) && (pte1 & (PTE_R|PTE_W|PTE_X)) == 0) { 
            uint64_t *pt1 = (uint64_t *)((pte1 >> 10) << 12);
            uint64_t *new_pt1 = (uint64_t *)pmm_alloc_frame();
            memset(new_pt1, 0, 4096);
            child_pt[i] = (((uint64_t)new_pt1 >> 12) << 10) | PTE_V;
            
            for (int j = 0; j < 512; j++) {
                uint64_t pte2 = pt1[j];
                if ((pte2 & PTE_V) && (pte2 & (PTE_R|PTE_W|PTE_X)) == 0) { 
                    uint64_t *pt2 = (uint64_t *)((pte2 >> 10) << 12);
                    uint64_t *new_pt2 = (uint64_t *)pmm_alloc_frame();
                    memset(new_pt2, 0, 4096);
                    new_pt1[j] = (((uint64_t)new_pt2 >> 12) << 10) | PTE_V;
                    
                    for (int k = 0; k < 512; k++) {
                        uint64_t pte3 = pt2[k];
                        if ((pte3 & PTE_V) && (pte3 & PTE_U)) { 
                            void *old_page = (void *)((pte3 >> 10) << 12);
                            void *new_page = pmm_alloc_frame();
                            uint64_t *src = (uint64_t *)old_page;
                            uint64_t *dst = (uint64_t *)new_page;
                            for(int b = 0; b < 512; b++) dst[b] = src[b]; 
                            new_pt2[k] = (((uint64_t)new_page >> 12) << 10) | (pte3 & 0x3FF);
                        }
                    }
                }
            }
        }
    }
    return child_pt;
}

// -------------------------------------------------------------
// 【重归稳健】：启用类似于 setjmp 的汇编快照，防止子进程丢毁 C 函数栈帧！
// -------------------------------------------------------------
asm(
    ".text\n"
    ".global capture_context\n"
    "capture_context:\n"
    "sd ra, 0(a0)\n"
    "sd sp, 8(a0)\n"
    "sd s0, 16(a0)\n"
    "sd s1, 24(a0)\n"
    "sd s2, 32(a0)\n"
    "sd s3, 40(a0)\n"
    "sd s4, 48(a0)\n"
    "sd s5, 56(a0)\n"
    "sd s6, 64(a0)\n"
    "sd s7, 72(a0)\n"
    "sd s8, 80(a0)\n"
    "sd s9, 88(a0)\n"
    "sd s10, 96(a0)\n"
    "sd s11, 104(a0)\n"
    "li a0, 0\n"       // 重点：当由 capture_context 直接返回时，返回值为 0。
    "ret\n"
);
extern uint64 capture_context(uint64 *ctx);

int fork(void) {
    struct proc *np = allocproc();
    if(np == 0) return -1;

    if (current->pagetable) {
        np->pagetable = clone_user_pagetable((uint64_t*)current->pagetable);
    } else {
        np->pagetable = 0;
    }
    np->ustack = current->ustack;
    np->entry = current->entry;

    uint64 p_base = current->kstack - 8192;
    uint64 p_top  = current->kstack;
    uint64 c_top  = np->kstack;

    // 完整拷贝内核栈
    uint64 *parent_stack = (uint64 *)p_base;
    uint64 *child_stack = (uint64 *)(c_top - 8192);
    for(uint64 i = 0; i < 8192UL / sizeof(uint64); i++) {
        child_stack[i] = parent_stack[i];
    }

    // 捕捉当前函数状态
    if (capture_context(np->context) == 0) {
        // --- 这里是父进程 ---
        
        // 1. 重写子进程 context 中的所有保存寄存器（修复内部指针偏移）
        for (int i = 1; i <= 13; i++) {
            uint64 val = np->context[i];
            if (val >= p_base && val <= p_top) {
                np->context[i] = c_top - (p_top - val);
            }
        }

        // 2. 深度重写子进程的内核栈数据，消除指向父栈的隐形毒药
        for (uint64 i = 0; i < 8192UL / sizeof(uint64); i++) {
            uint64 val = child_stack[i];
            if (val >= p_base && val <= p_top) {
                child_stack[i] = c_top - (p_top - val);
            }
        }

        np->state = RUNNABLE;
        np->parent = current;
        return np->pid;
    } else {
        // --- 这里是子进程 ---
        // 子进程通过 swtch 苏醒后，其 a0 (返回值) 并非为 0。
        // 它会跳入这里并执行 return 0，从而安全执行完整的函数退栈指令（Epilogue），绝不会再引发内核跑飞！
        return 0; 
    }
}

int exec(const char *path, char *const argv[], uint64_t *out_argc, uint64_t *out_argv) {
    int fd = fs_open(path, O_RDONLY);
    if (fd < 0) return -1;

    extern void *kmalloc(uint64_t size);
    extern void kfree(void *ptr);
    uint8_t *elf_buf = (uint8_t *)kmalloc(65536);
    if (!elf_buf) { fs_close(fd); return -1; }

    int size = fs_read(fd, elf_buf, 65536);
    fs_close(fd);
    if (size <= 0) { kfree(elf_buf); return -1; }

    uint32_t entry, user_stack_ignored;
    if (load_elf(elf_buf, &entry, &user_stack_ignored) != 0) { kfree(elf_buf); return -1; }

    uint64_t *user_pagetable = (uint64_t *)pmm_alloc_frame();
    if (!user_pagetable) { kfree(elf_buf); return -1; }
    memset(user_pagetable, 0, 4096); 
    
    extern uint64_t *kernel_pagetable;
    for (int i = 0; i < 512; i++) {
        user_pagetable[i] = kernel_pagetable[i];
    }

    extern void load_elf_map(uint64_t *pagetable, const void *elf_data);
    load_elf_map(user_pagetable, elf_buf);
    kfree(elf_buf);

    uint64_t ustack_top = 0x7F000000; 
    uint64_t ustack_bottom = ustack_top - 4096;
    uint64_t stack_pa = (uint64_t)pmm_alloc_frame();
    if (!stack_pa) return -1;
    
    vmm_map_page(user_pagetable, ustack_bottom, stack_pa, PTE_R | PTE_W | PTE_U);

    uint64_t argc = 0;
    while (argv && argv[argc]) argc++;

    uint64_t sp = ustack_top;
    uint64_t user_argv_addrs[16];

    for (int i = argc - 1; i >= 0; i--) {
        int len = 0;
        while(argv[i][len]) len++;
        len++; 

        sp -= len;
        char *p = (char *)(stack_pa + (sp - ustack_bottom));
        for (int j = 0; j < len; j++) p[j] = argv[i][j];
        user_argv_addrs[i] = sp;
    }

    // 【极其核心】：RISC-V 规定 sp 必须严格保证 16 字节对齐！
    // 否则程序入栈时极大概率越过 0x7F000000 的上边界，直接导致 0x7f000008 缺页崩溃！
    sp &= ~15ULL; 
    sp -= (argc + 1) * sizeof(uint64_t);
    sp &= ~15ULL; // 减去数组大小后，再次强制 16 字节对齐护航

    uint64_t *p_argv = (uint64_t *)(stack_pa + (sp - ustack_bottom));
    for (int i = 0; i < argc; i++) {
        p_argv[i] = user_argv_addrs[i];
    }
    p_argv[argc] = 0;

    current->pagetable = user_pagetable;
    current->ustack = sp;
    current->entry = entry;

    if (out_argc) *out_argc = argc;
    if (out_argv) *out_argv = sp;

    return 0;
}

void sem_init(struct semaphore *sem, int value) {
    sem->count = value;
    sem->chan = sem;
}

void sem_wait(struct semaphore *sem) {
    while(sem->count <= 0) blocked(sem->chan);
    sem->count--;
}

void sem_signal(struct semaphore *sem) {
    sem->count++;
    wakeup(sem->chan);
}

static void init_process(void) {
    printf("\n[Init] Starting shell...\n");
    extern void shell_loop(void);
    shell_loop();
}

void proc_init(void) {
    for (int i = 0; i < NPROC; i++) procs[i].state = UNUSED;
    create_proc(init_process);
    current = scheduler();
    if(current) {
        current->state = RUNNING;
        current->slice = (sched_algorithm == 2) ? 1 : 10;
        asm volatile(
            "ld sp, 8(%0)\n"
            "ld ra, 0(%0)\n"
            "ret\n"
            : : "r"(current->context) : "memory"
        );
    }
}