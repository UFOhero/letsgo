
#include "heap.h"
#include "pmm.h"
#include "vmm.h"

extern uint64_t *kernel_pagetable;
extern void panic(const char *msg);
extern int printf(const char *fmt, ...);

// 我们把内核动态堆的起点定在 0xC0000000，和 0x80000000 的内核代码区分开
#define HEAP_START_VA 0xC0000000

// 内存块的头部“元数据” (16 bytes)
struct block_header {
    uint64_t size;             // 这个块的总大小 (包含 header 本身)
    int is_free;               // 1: 空闲, 0: 占用
    struct block_header *next; // 指向下一个块的指针
};

static struct block_header *free_list = NULL;
static uint64_t current_heap_end = HEAP_START_VA; // 记录当前虚拟堆生长到了哪里

// 当堆内存不够时，向系统申请物理页并映射到虚拟地址
static void expand_heap(uint64_t size) {
    // 向上取整，计算需要多少个 4KB 物理页
    uint64_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t start_va = current_heap_end;

    for (uint64_t i = 0; i < pages_needed; i++) {
        // 1. 拿一个真实的物理页
        uint64_t pa = (uint64_t)pmm_alloc_frame();
        if (!pa) panic("[Heap] Out of Physical Memory!");
        
        // 2. 【魔法时刻】：无论物理页多乱，我们把它按顺序拼接到 current_heap_end 上！
        vmm_map_page(kernel_pagetable, current_heap_end, pa, PTE_R | PTE_W);
        current_heap_end += PAGE_SIZE;
    }
    
    // 刷新 TLB，因为我们改了页表
    __asm__ volatile("sfence.vma zero, zero");

    // 把新要来的这一大片连续虚拟内存，做成一个大的 block，塞进空闲链表
    struct block_header *new_block = (struct block_header *)start_va;
    new_block->size = pages_needed * PAGE_SIZE;
    new_block->is_free = 1;
    new_block->next = free_list;
    free_list = new_block;
}

void heap_init(void) {
    // 初始化时先申请一页 (4096 bytes) 备用
    expand_heap(PAGE_SIZE);
    printf("[Heap] Initialized at Virtual Address: 0x%lx\n", HEAP_START_VA);
}

void *kmalloc(uint64_t size) {
    if (size == 0) return NULL;

    // 内存对齐：为了防止 CPU 访问结构体时不对齐引发崩溃，强制 8 字节对齐
    uint64_t total_size = (size + 7) & ~7; 
    total_size += sizeof(struct block_header); // 算上元数据头的大小

    struct block_header *curr = free_list;

    // First-Fit 算法：遍历链表，找到第一个够大的空闲块
    while (curr != NULL) {
        if (curr->is_free && curr->size >= total_size) {
            
            // 如果这个块太大了（剩余空间还能再放下一个 header + 至少8字节），我们就把它“切开”
            if (curr->size >= total_size + sizeof(struct block_header) + 8) {
                struct block_header *new_block = (struct block_header *)((uint64_t)curr + total_size);
                new_block->size = curr->size - total_size;
                new_block->is_free = 1;
                new_block->next = curr->next;
                
                curr->size = total_size;
                curr->next = new_block;
            }
            
            curr->is_free = 0; // 标记为被占用
            
            // 返回“去除 header 后”的纯粹数据区地址给程序用
            return (void *)((uint64_t)curr + sizeof(struct block_header));
        }
        curr = curr->next;
    }

    // 如果遍历完发现没有足够大的块？不用怕，让堆扩张！
    expand_heap(total_size);
    
    // 扩张完了，重新调用自己，这次一定能分配成功
    return kmalloc(size); 
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    // 指针往回倒退一个 header 的大小，找到元数据
    struct block_header *block = (struct block_header *)((uint64_t)ptr - sizeof(struct block_header));
    
    // 第一版为了求稳，我们暂时不合并碎片，只做简单的标记释放
    block->is_free = 1;
}