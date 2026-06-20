CC = riscv64-unknown-elf-gcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy
XXD = xxd

CFLAGS = -mcmodel=medany -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -I. -g -O0
LDFLAGS = -T link.ld

# 内核对象（移除 userlib.o）
KERNEL_OBJS = start.o kernel.o uart.o plic.o timer.o sbi.o \
              pmm.o vmm.o heap.o string.o trap_vec.o trap.o syscall.o \
              proc.o swtch.o elf.o fs.o

USER_DIR = user
USER_SRCS = $(wildcard $(USER_DIR)/*.c)
USER_ELFS = $(USER_SRCS:.c=.elf)
USER_ARRAY_C = $(USER_SRCS:.c=_array.c)
USER_ARRAY_O = $(USER_SRCS:.c=_array.o)

KERNEL_OBJS += $(USER_ARRAY_O)

all: kernel.elf $(USER_ELFS)

kernel.elf: $(KERNEL_OBJS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# 构建用户程序 ELF
$(USER_DIR)/%.elf: $(USER_DIR)/%.c userlib.o user.ld
	@mkdir -p $(USER_DIR)
	$(CC) $(CFLAGS) -I. -c $< -o $(basename $@).o
	$(LD) -T user.ld -o $@ $(basename $@).o userlib.o

userlib.o: userlib.c userlib.h syscall.h
	$(CC) $(CFLAGS) -I. -c userlib.c -o userlib.o

# 从 ELF 生成数组 C 文件
$(USER_DIR)/%_array.c: $(USER_DIR)/%.elf
	$(XXD) -i $< | sed 's/unsigned char .*_elf\[\]/unsigned char $*_elf\[\]/' | \
	sed 's/unsigned int .*_elf_len/unsigned int $*_elf_len/' > $@

# 编译数组文件
$(USER_DIR)/%_array.o: $(USER_DIR)/%_array.c
	$(CC) $(CFLAGS) -c $< -o $@

run: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -m 256M -kernel kernel.elf

clean:
	rm -f *.o kernel.elf $(USER_DIR)/*.o $(USER_DIR)/*.elf $(USER_DIR)/*_array.c $(USER_DIR)/*_array.o