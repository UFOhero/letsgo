// elf.c 完整修改后代码
#include "elf.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"

extern uint64_t *kernel_pagetable;

int load_elf(const void *elf_data, uint32_t *entry, uint32_t *user_stack) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*)elf_data;
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F')
        return -1;

    *entry = (uint32_t)ehdr->e_entry;
    *user_stack = 0x7FFFF000;    // 用户栈顶
    return 0;
}

void load_elf_map(uint64_t *pagetable, const void *elf_data) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*)elf_data;
    const Elf64_Phdr *phdr = (const Elf64_Phdr*)((const uint8_t*)elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint64_t vaddr   = phdr[i].p_vaddr;
        uint64_t filesz  = phdr[i].p_filesz;
        uint64_t memsz   = phdr[i].p_memsz;
        uint64_t offset  = phdr[i].p_offset;

        uint64_t va_start = vaddr & ~0xFFFULL;
        uint64_t va_end   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;

        // 根据 ELF 段标志生成页表权限
        uint64_t perm = PTE_U;                       // 用户态必须设置 U 位
        if (phdr[i].p_flags & 1)  perm |= PTE_X;     // PF_X -> 可执行
        if (phdr[i].p_flags & 2)  perm |= PTE_W;     // PF_W -> 可写
        if (phdr[i].p_flags & 4)  perm |= PTE_R;     // PF_R -> 可读

        for (uint64_t va = va_start; va < va_end; va += PAGE_SIZE) {
            void *page = pmm_alloc_frame();
            if (!page) return;
            vmm_map_page(pagetable, va, (uint64_t)page, perm);

            // 拷贝文件数据
            uint64_t pa = (uint64_t)page;
            uint64_t copy_start_va = (va > vaddr) ? va : vaddr;
            uint64_t copy_end_va   = (va + PAGE_SIZE < vaddr + filesz) 
                                        ? (va + PAGE_SIZE) : (vaddr + filesz);

            if (copy_start_va < copy_end_va) {
                uint64_t file_off = offset + (copy_start_va - vaddr);
                uint64_t len = copy_end_va - copy_start_va;
                memcpy((void*)(pa + (copy_start_va - va)),
                       (const uint8_t*)elf_data + file_off, len);
            }
            // pmm_alloc_frame 已清零，.bss 无需额外处理
        }
    }
}

// 全局变量，供 trap_vec.S 使用
volatile int switch_to_user = 0;
uint64_t user_satp = 0;