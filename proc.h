#ifndef PROC_H
#define PROC_H
#include<stdint.h>
#include"syscall.h"   //引入struct trapframe

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

//最大进程数
#define NPROC  8
#define NQUEUE 3     //MLFQ队列的队列数量  

//时间片设置
#define TIME_SLICE_COUNT 1     //时间片计数

//进程状态
enum procstate {
    UNUSED,    // 空闲
    RUNNABLE,  // 就绪
    RUNNING,   // 执行
    BLOCKED,   // 阻塞
    ZOMBIE     // 僵尸
};

//PCB
struct proc {
    uint64 pid;             // 进程ID
    enum procstate state;   //当前进程状态
    uint64 context[32];     // 保存寄存器上下文
    uint64 kstack;          // 内核栈指针
    uint64 ctime;           // 创建时间
    uint64 enq_time;        // 入队时间
    uint64 slice;           // 剩余时间片
    int priority;           //优先级(0为最高，NQUEUE - 1最低)
    struct proc *parent;    // 父进程指针
    volatile int exit_code;          // 退出码
    void *chan;             // 阻塞等待通道
    uint64_t *pagetable;       // 用户页表
    uint64_t ustack;           // 用户栈顶
    uint64_t entry;            // 用户入口
};

//信号量
struct semaphore {
    int count;    //可用资源数
    void *chan;   //等待队列
};

//自旋锁
typedef volatile int spinlock_t;

//初始化自旋锁（为未锁定状态）
static inline void spin_init(spinlock_t *lk) { *lk = 0; }
//获取自旋锁
static inline void spin_lock(spinlock_t *lk) { while(__sync_lock_test_and_set(lk, 1)); }
//释放自旋锁
static inline void spin_unlock(spinlock_t *lk) { __sync_lock_release(lk); }

extern struct proc procs[NPROC];    //进程表
extern struct proc *current;        //当前运行的进程
extern int sched_algorithm;         //调度算法：0=RR, 1=FCFS
extern volatile int need_resched;   //是否需要调度（时钟中断设置）

void proc_init(void);  //初始化进程
void yield(void);  //主动放弃CPU
void sched_tick(void);  //时钟中断
void blocked(void *chan);  //阻塞进程
void wakeup(void *chan);  //唤醒进程
int  create_proc(void (*func)(void));  //创建内核级进程
void exit();  //终止进程
int  wait(int pid, int *status);  //等待子进程退出
int fork(void);  //复制当前进程
int fork_from_trap(struct trapframe *tf);

void swtch(uint64 *old_context, uint64 *new_context);  // 切换上下文
void sem_init(struct semaphore *sem, int value);
void sem_wait(struct semaphore *sem);
void sem_signal(struct semaphore *sem);
void schedule(struct trapframe *tf);

void uart_putc(char c);
void uart_puts(const char *str);
int printf(const char *fmt, ...);
int exec(const char *path, char *const argv[], uint64_t *out_argc, uint64_t *out_argv);

void do_ps(void);
int do_kill(int pid);

#endif
