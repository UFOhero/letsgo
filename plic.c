#include <stdint.h>

// 针对 QEMU virt 主板上 Hart 0 的 S-Mode 地址硬编码
#define PLIC_PRIORITY ((volatile uint32_t *)0x0C000000)
#define PLIC_SENABLE  ((volatile uint32_t *)0x0C002080)
#define PLIC_STHRESH  ((volatile uint32_t *)0x0C201000)
#define PLIC_SCLAIM   ((volatile uint32_t *)0x0C201004)

void plic_init(void) {
    // 1. 设置 UART0 (IRQ 10) 的优先级为 1 (必须大于 0 才会触发)
    PLIC_PRIORITY[10] = 1;
    
    // 2. 允许 Hart 0 的 S-Mode 接收 IRQ 10 的中断信号
    *PLIC_SENABLE = (1 << 10);
    
    // 3. 设置 S-Mode 的中断阈值为 0 (任何优先级 > 0 的中断都能放行)
    *PLIC_STHRESH = 0;
}

// 认领中断：询问 PLIC 是哪个 IRQ 触发了中断
uint32_t plic_claim(void) {
    return *PLIC_SCLAIM;
}

// 完成中断：告诉 PLIC 该 IRQ 已经处理完毕
void plic_complete(uint32_t irq) {
    *PLIC_SCLAIM = irq;
}