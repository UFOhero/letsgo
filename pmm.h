#ifndef _PMM_H
#define _PMM_H
#include <stdint.h>

#define PAGE_SIZE 4096
#define NULL ((void *)0)

// 阶段二的核心接口
void pmm_init(uint64_t mem_start, uint64_t mem_end);
void *pmm_alloc_frame(void);
void pmm_free_frame(void *pa);
void pmm_test(void);

#endif
