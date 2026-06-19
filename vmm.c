#include "vmm.h"
#include "pmm.h"

extern void panic(const char *msg);
extern int printf(const char *fmt, ...);

uint64_t *kernel_pagetable;

void vmm_map_page(uint64_t *pagetable, uint64_t va, uint64_t pa, uint64_t flags) {
    uint64_t vpn[3];
    vpn[0] = (va >> 12) & 0x1FF;
    vpn[1] = (va >> 21) & 0x1FF;
    vpn[2] = (va >> 30) & 0x1FF;

    uint64_t *current_table = pagetable;

    for (int level = 2; level > 0; level--) {
        uint64_t *pte = &current_table[vpn[level]]; 
        
        if (*pte & PTE_V) {
            if (*pte & (PTE_R | PTE_W | PTE_X)) {
                panic("vmm_map_page: Encountered a leaf PTE instead of directory!");
            }
            uint64_t ppn = (*pte) >> 10;
            current_table = (uint64_t *)(ppn << 12);
        } else {
            uint64_t *new_table = (uint64_t *)pmm_alloc_frame();
            if (!new_table) {
                panic("vmm_map_page: OOM for page tables");
            }
            
            *pte = (((uint64_t)new_table >> 12) << 10) | PTE_V;
            current_table = new_table;
        }
    }

    uint64_t *leaf_pte = &current_table[vpn[0]];
    if (*leaf_pte & PTE_V) {
        printf("Conflict VA: 0x%lx\n", va);
        panic("vmm_map_page: Virtual address already mapped!");
    }
    
    *leaf_pte = ((pa >> 12) << 10) | flags | PTE_V | PTE_A | PTE_D;
}

void vmm_init(void) {
    kernel_pagetable = (uint64_t *)pmm_alloc_frame();

    // 1. 映射串口 (UART)
    vmm_map_page(kernel_pagetable, 0x10000000, 0x10000000, PTE_R | PTE_W);

    // 【核心修复】：2. 映射中断控制器 (PLIC)
    // PLIC 的寄存器分散在几个特定的页里，我们必须为这些页授予读写权限，否则会触发 Cause 15
    vmm_map_page(kernel_pagetable, 0x0C000000, 0x0C000000, PTE_R | PTE_W); // 映射 PLIC_PRIORITY 所在的页
    vmm_map_page(kernel_pagetable, 0x0C002000, 0x0C002000, PTE_R | PTE_W); // 映射 PLIC_SENABLE 所在的页
    vmm_map_page(kernel_pagetable, 0x0C201000, 0x0C201000, PTE_R | PTE_W); // 映射 PLIC_STHRESH 和 SCLAIM 所在的页

    // 3. 映射物理内存 (RAM)
    uint64_t phys_mem_start = 0x80000000;
    uint64_t phys_mem_end   = 0x88000000;
    
    for (uint64_t addr = phys_mem_start; addr < phys_mem_end; addr += PAGE_SIZE) {
        vmm_map_page(kernel_pagetable, addr, addr, PTE_R | PTE_W | PTE_X);
    }

    printf("[VMM] Page tables constructed.\n");

    uint64_t satp = MAKE_SATP(kernel_pagetable);
    __asm__ volatile("csrw satp, %0" : : "r" (satp));
    __asm__ volatile("sfence.vma zero, zero");

    printf("[VMM] Paging activated. We are in Virtual Memory now!\n");
}

// 在用户/内核页表中查找虚拟地址对应的物理地址（含页内偏移），若不存在返回 0
uint64_t vmm_lookup_page(uint64_t *pagetable, uint64_t va) {
    uint64_t vpn[3];
    vpn[0] = (va >> 12) & 0x1FF;
    vpn[1] = (va >> 21) & 0x1FF;
    vpn[2] = (va >> 30) & 0x1FF;

    uint64_t *current_table = pagetable;

    for (int level = 2; level >= 0; level--) {
        uint64_t pte = current_table[vpn[level]];
        if (!(pte & PTE_V))
            return 0;

        // 如果是叶节点（设置了 R/W/X 权限位）
        if (pte & (PTE_R | PTE_W | PTE_X)) {
            uint64_t ppn = pte >> 10;
            return (ppn << 12) | (va & 0xFFF);   // 完整的物理地址
        }

        // 非叶节点，进入下一级
        uint64_t ppn = pte >> 10;
        current_table = (uint64_t *)(ppn << 12);
    }
    return 0;
}