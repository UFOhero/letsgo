#include <stdint.h>
#include "proc.h"
#include "heap.h"
#include "csr.h"
#include "pmm.h"
#include "vmm.h"
#include "elf.h"
#include "fs.h"
#include "string.h"

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
            memset(&procs[i], 0, sizeof(struct proc));   // 彻底清零
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

/* 调度器算法省略，同前 */
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
                if (status) *status = procs[i].exit_code;
                procs[i].state = UNUSED;
                return ret_pid;
            }
        }
        if (!have_kids) return -1;
        blocked(current);
    }
}

int fork(void) {
    struct proc *np;
    uint64 ra, sp;
    uint64 s_regs[12];

    asm volatile("mv %0, ra" : "=r"(ra));
    asm volatile("mv %0, sp" : "=r"(sp));
    np = allocproc();
    if(np == 0) return -1;

    // 【新增这三行】：继承父进程的页表和虚拟地址空间，确保子进程能回到用户态
    np->pagetable = current->pagetable;
    np->ustack = current->ustack;
    np->entry = current->entry;

    asm volatile("mv %0, s0"  : "=r"(s_regs[0]));
    asm volatile("mv %0, s1"  : "=r"(s_regs[1]));
    asm volatile("mv %0, s2"  : "=r"(s_regs[2]));
    asm volatile("mv %0, s3"  : "=r"(s_regs[3]));
    asm volatile("mv %0, s4"  : "=r"(s_regs[4]));
    asm volatile("mv %0, s5"  : "=r"(s_regs[5]));
    asm volatile("mv %0, s6"  : "=r"(s_regs[6]));
    asm volatile("mv %0, s7"  : "=r"(s_regs[7]));
    asm volatile("mv %0, s8"  : "=r"(s_regs[8]));
    asm volatile("mv %0, s9"  : "=r"(s_regs[9]));
    asm volatile("mv %0, s10" : "=r"(s_regs[10]));
    asm volatile("mv %0, s11" : "=r"(s_regs[11]));

    uint64 *parent_stack = (uint64 *)(current->kstack - 8192);
    uint64 *child_stack = (uint64 *)(np->kstack - 8192);
    for(uint64 i = 0; i < 8192UL / sizeof(uint64); i++) child_stack[i] = parent_stack[i];

    for(int i = 0; i < 32; i++) np->context[i] = 0;
    np->context[0] = ra;
    uint64 offset = current->kstack - sp;
    np->context[1] = np->kstack - offset;
    
    // 【核心修复】：重新定位子进程的帧指针 (Frame Pointers)
    // 防止子进程通过 s0 访问/修改到父进程的局部变量区！
    for(int i = 0; i < 12; i++) {
        uint64 val = s_regs[i];
        if (val >= (current->kstack - 8192) && val <= current->kstack) {
            val = np->kstack - (current->kstack - val);
        }
        np->context[2 + i] = val;
    }
    
    np->context[14] = 0;
    np->state = RUNNABLE;
    np->parent = current;
    return np->pid;
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

    // 我们依然解析 ELF，但忽略它推荐的 user_stack
    uint32_t entry, user_stack_ignored;
    if (load_elf(elf_buf, &entry, &user_stack_ignored) != 0) { kfree(elf_buf); return -1; }

    uint64_t *user_pagetable = (uint64_t *)pmm_alloc_frame();
    if (!user_pagetable) { kfree(elf_buf); return -1; }
    
    extern uint64_t *kernel_pagetable;
    for (int i = 0; i < 512; i++) {
        user_pagetable[i] = kernel_pagetable[i];
    }

    extern void load_elf_map(uint64_t *pagetable, const void *elf_data);
    load_elf_map(user_pagetable, elf_buf);
    kfree(elf_buf);

    // 【核心修复1】：强行规定用户的虚拟栈顶为 0x7F000000（保证 4KB 绝对页对齐）
    // 这能让物理偏移和虚拟偏移完美匹配！
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
        len++; // 包含 '\0'

        sp -= len;
        char *p = (char *)(stack_pa + (sp - ustack_bottom));
        for (int j = 0; j < len; j++) p[j] = argv[i][j];
        user_argv_addrs[i] = sp;
    }

    sp &= ~7ULL;
    sp -= (argc + 1) * sizeof(uint64_t);
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