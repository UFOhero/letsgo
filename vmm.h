#ifndef _VMM_H
#define _VMM_H
#include <stdint.h>

#define PAGE_SIZE 4096

#define PA2VA(pa) ((uint64_t)(pa))  
#define VA2PA(va) ((uint64_t)(va))

// 页表项 (PTE) 的权限位标志
#define PTE_V (1L << 0) // Valid (有效位)
#define PTE_R (1L << 1) // Read (可读)
#define PTE_W (1L << 2) // Write (可写)
#define PTE_X (1L << 3) // Execute (可执行)
#define PTE_U (1L << 4) // User (用户态可访问)
#define PTE_A (1L << 6) // Accessed (已访问，硬件自动设置，我们初始化时可默认给 1 防止部分硬件缺页)
#define PTE_D (1L << 7) // Dirty (已写过，默认给 1)

// RISC-V Sv39 的 satp 寄存器构造宏
// Mode 8 表示开启 Sv39 分页，PPN 是根页表的物理页号
#define MAKE_SATP(pagetable_pa) ( (8ULL << 60) | ((uint64_t)(pagetable_pa) >> 12) )

void vmm_init(void);
void vmm_map_page(uint64_t *pagetable, uint64_t va, uint64_t pa, uint64_t flags);
uint64_t vmm_lookup_page(uint64_t *pagetable, uint64_t va);

#endif