
#include "pmm.h"

extern int printf(const char *fmt, ...);
extern void panic(const char *msg);

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t page_start_addr; // 去掉位图占用后，真正可分配的起始物理地址

// 位操作宏
#define BITMAP_SET(idx)   (((volatile uint8_t *)bitmap)[(idx) / 8] |=  (1 << ((idx) % 8)))
#define BITMAP_CLEAR(idx) (((volatile uint8_t *)bitmap)[(idx) / 8] &= ~(1 << ((idx) % 8)))
#define BITMAP_TEST(idx)  (((volatile uint8_t *)bitmap)[(idx) / 8] &   (1 << ((idx) % 8)))

void pmm_init(uint64_t mem_start, uint64_t mem_end) {
    // 1. 计算总页数
    uint64_t total_bytes = mem_end - mem_start;
    total_pages = total_bytes / PAGE_SIZE;

    // 2. 计算位图本身需要占据多少字节
    uint64_t bitmap_bytes = total_pages / 8;
    if (total_pages % 8 != 0) {
        bitmap_bytes++;
    }

    // 3. 将位图放置在可用内存的最开头
    bitmap = (uint8_t *)mem_start;

    // 4. 清空位图：0 表示全部空闲
    // 强制真实写入，不准编译器把清零循环优化为 Dead Store
    volatile uint8_t *v_bitmap = (volatile uint8_t *)bitmap;
    for (uint64_t i = 0; i < bitmap_bytes; i++) {
        v_bitmap[i] = 0;
    }

    // 5. 计算位图占据了多少个物理页（向上取整）
    uint64_t bitmap_occupied_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    // 6. 核心“自举”逻辑：在位图中，将位图自己占据的页标记为“已分配”
    for (uint64_t i = 0; i < bitmap_occupied_pages; i++) {
        BITMAP_SET(i);
    }

    page_start_addr = mem_start; 

    printf("[PMM] Init Done. Total Pages: %ld, Bitmap occupied: %ld pages\n", 
           total_pages, bitmap_occupied_pages);
}

// 分配一个物理页框，返回物理地址
void *pmm_alloc_frame(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!BITMAP_TEST(i)) {
            // 找到空闲页，标记为已分配
            BITMAP_SET(i);
            
            // 计算其实际物理地址
            uint64_t allocated_pa = page_start_addr + (i * PAGE_SIZE);
            
            // 强制清空物理页，杜绝上一轮的垃圾数据欺骗 MMU
            volatile uint64_t *clear_ptr = (volatile uint64_t *)allocated_pa;
            for (int j = 0; j < PAGE_SIZE / 8; j++) {
                clear_ptr[j] = 0;
            }
            __asm__ volatile("" : : : "memory");

            return (void *)allocated_pa;
        }
    }
    panic("Out of Physical Memory!");
    return NULL;
}

// 释放一个物理页框
void pmm_free_frame(void *pa) {
    uint64_t addr = (uint64_t)pa;
    
    // 地址合法性检查
    if (addr < page_start_addr || addr >= (page_start_addr + total_pages * PAGE_SIZE)) {
        panic("pmm_free: invalid physical address");
    }
    if (addr % PAGE_SIZE != 0) {
        panic("pmm_free: address not aligned to 4KB");
    }

    // 计算索引并清除位状态
    uint64_t idx = (addr - page_start_addr) / PAGE_SIZE;
    if (!BITMAP_TEST(idx)) {
        panic("pmm_free: double free detected!");
    }
    
    BITMAP_CLEAR(idx);
}