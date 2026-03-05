# Makefile for RISC-V Kernel with Bootloader
# Logisim RISC-V Computer System model by Pavel Gladyshev
# Licensed under Creative Commons Attribution International License 4.0
#
# Directory structure:
#   kernel/  - Kernel source files and headers
#   boot/    - Bootloader source files
#   user/    - User programs and runtime library
#   fstool/  - Native filesystem tool for host
#
# To install required packages in Ubuntu Linux:
#   sudo apt install build-essential binutils-riscv64-unknown-elf gcc-riscv64-unknown-elf

# ====================================
# Common configuration
# ====================================

RISCV_TOOL_PREFIX = /opt/homebrew/bin/riscv64-elf-
RISCV_ISA = rv32im_zicsr
RISCV_ABI = ilp32
CFLAGS = -Wno-builtin-declaration-mismatch
QEMU_APP = qemu-system-riscv64
NATIVE_CC = gcc

# Common RISC-V compilation flags
RISCV_CFLAGS = -O -march=$(RISCV_ISA) -mabi=$(RISCV_ABI) -mcmodel=medany -g $(CFLAGS)

# Default target: build bootloader ROM image
.DEFAULT_GOAL := boot-rom.txt

# ====================================
# Bootloader build
# ====================================

BOOT_OBJS = boot/boot_crt0.o boot/boot_main.o boot/boot_io.o \
            boot/boot_fs.o boot/boot_string.o

BOOT_HEADERS = boot/boot.h kernel/types.h kernel/fs_types.h

boot/%.o: boot/%.c $(BOOT_HEADERS)
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

boot/%.o: boot/%.S $(BOOT_HEADERS)
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

bootloader: $(BOOT_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o bootloader -Map bootloader.map -T boot/boot.lds $(BOOT_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S bootloader > bootloader.asm
	$(RISCV_TOOL_PREFIX)nm bootloader > bootloader.sym

boot-rom: bootloader
	$(RISCV_TOOL_PREFIX)objcopy --only-section .init --only-section .text --only-section .rodata \
		--only-section .srodata --only-section .data --only-section .sdata \
		--only-section .text.* --only-section .rodata.* --only-section .srodata.* \
		--only-section .data.* --only-section .sdata.* \
		--output-target binary bootloader boot-rom

boot-rom.txt: boot-rom
	echo "v2.0 raw" > boot-rom.txt
	hexdump -v -e '/4 "%08x""\n"""' boot-rom >> boot-rom.txt

# ====================================
# Kernel build (for RAM, loaded by bootloader)
# ====================================

KERNEL_OBJS = kernel/crt0.o kernel/string.o kernel/console.o kernel/block.o \
              kernel/inode.o kernel/dir.o kernel/file.o kernel/fs.o \
              kernel/device.o kernel/console_dev.o kernel/loader.o \
              kernel/loader_asm.o kernel/trap.o kernel/process.o \
              kernel/syscall.o kernel/main.o

KERNEL_HEADERS = kernel/fs.h kernel/types.h kernel/fs_types.h kernel/string.h \
                 kernel/console.h kernel/block.h kernel/inode.h kernel/dir.h \
                 kernel/file.h kernel/device.h kernel/console_dev.h kernel/elf.h \
                 kernel/loader.h kernel/trap.h kernel/syscall.h kernel/process.h

kernel/%.o: kernel/%.c $(KERNEL_HEADERS)
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

kernel/%.o: kernel/%.S $(KERNEL_HEADERS)
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

kernel-debug.elf: $(KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o kernel-debug.elf -Map kernel.map -T kernel/kernel.lds $(KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S kernel-debug.elf > kernel.asm
	$(RISCV_TOOL_PREFIX)nm kernel-debug.elf > kernel.sym

kernel.bin: kernel-debug.elf
	$(RISCV_TOOL_PREFIX)objcopy --output-target binary kernel-debug.elf kernel.bin

# ====================================
# User programs (loaded by kernel with syscall support)
# ====================================

USER_LINKER_SCRIPT = user/user.lds

# Flags for position-independent user programs
USER_CFLAGS = -fPIE -fno-jump-tables $(RISCV_CFLAGS)

USER_LIB_OBJS = user/syscall.user.o user/libc.user.o
USER_COMMON_OBJS = user/user_crt0.user.o $(USER_LIB_OBJS)

user/%.user.o: user/%.c user/libc.h
	$(RISCV_TOOL_PREFIX)gcc $(USER_CFLAGS) -Iuser -c $< -o $@

user/%.user.o: user/%.S kernel/syscall_nr.h
	$(RISCV_TOOL_PREFIX)gcc $(USER_CFLAGS) -Iuser -Ikernel -c $< -o $@

# Hello program
HELLO_OBJS = $(USER_COMMON_OBJS) user/hello.user.o

hello.elf: $(HELLO_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o hello-debug.elf -T $(USER_LINKER_SCRIPT) $(HELLO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S hello-debug.elf > hello.asm
	$(RISCV_TOOL_PREFIX)nm hello-debug.elf > hello.sym
	$(RISCV_TOOL_PREFIX)strip -s hello-debug.elf -o hello.elf

# Spawn demo program
SPAWN_DEMO_OBJS = $(USER_COMMON_OBJS) user/spawn_demo.user.o

spawn_demo.elf: $(SPAWN_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o spawn_demo-debug.elf -T $(USER_LINKER_SCRIPT) $(SPAWN_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S spawn_demo-debug.elf > spawn_demo.asm
	$(RISCV_TOOL_PREFIX)nm spawn_demo-debug.elf > spawn_demo.sym
	$(RISCV_TOOL_PREFIX)strip -s spawn_demo-debug.elf -o spawn_demo.elf

# Shell program
SHELL_OBJS = $(USER_COMMON_OBJS) user/shell.user.o

shell.elf: $(SHELL_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o shell-debug.elf -T $(USER_LINKER_SCRIPT) $(SHELL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S shell-debug.elf > shell.asm
	$(RISCV_TOOL_PREFIX)nm shell-debug.elf > shell.sym
	$(RISCV_TOOL_PREFIX)strip -s shell-debug.elf -o shell.elf

# Ls program
LS_OBJS = $(USER_COMMON_OBJS) user/ls.user.o

ls.elf: $(LS_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o ls-debug.elf -T $(USER_LINKER_SCRIPT) $(LS_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S ls-debug.elf > ls.asm
	$(RISCV_TOOL_PREFIX)nm ls-debug.elf > ls.sym
	$(RISCV_TOOL_PREFIX)strip -s ls-debug.elf -o ls.elf

# Mkdir program
MKDIR_OBJS = $(USER_COMMON_OBJS) user/mkdir.user.o

mkdir.elf: $(MKDIR_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o mkdir-debug.elf -T $(USER_LINKER_SCRIPT) $(MKDIR_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S mkdir-debug.elf > mkdir.asm
	$(RISCV_TOOL_PREFIX)nm mkdir-debug.elf > mkdir.sym
	$(RISCV_TOOL_PREFIX)strip -s mkdir-debug.elf -o mkdir.elf

# Rmdir program
RMDIR_OBJS = $(USER_COMMON_OBJS) user/rmdir.user.o

rmdir.elf: $(RMDIR_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o rmdir-debug.elf -T $(USER_LINKER_SCRIPT) $(RMDIR_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S rmdir-debug.elf > rmdir.asm
	$(RISCV_TOOL_PREFIX)nm rmdir-debug.elf > rmdir.sym
	$(RISCV_TOOL_PREFIX)strip -s rmdir-debug.elf -o rmdir.elf

# Mknod program
MKNOD_OBJS = $(USER_COMMON_OBJS) user/mknod.user.o

mknod.elf: $(MKNOD_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o mknod-debug.elf -T $(USER_LINKER_SCRIPT) $(MKNOD_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S mknod-debug.elf > mknod.asm
	$(RISCV_TOOL_PREFIX)nm mknod-debug.elf > mknod.sym
	$(RISCV_TOOL_PREFIX)strip -s mknod-debug.elf -o mknod.elf

# Env demo program
ENV_DEMO_OBJS = $(USER_COMMON_OBJS) user/env_demo.user.o

env_demo.elf: $(ENV_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o env_demo-debug.elf -T $(USER_LINKER_SCRIPT) $(ENV_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S env_demo-debug.elf > env_demo.asm
	$(RISCV_TOOL_PREFIX)nm env_demo-debug.elf > env_demo.sym
	$(RISCV_TOOL_PREFIX)strip -s env_demo-debug.elf -o env_demo.elf

# Cat program (test file reading)
CAT_OBJS = $(USER_COMMON_OBJS) user/cat.user.o

cat.elf: $(CAT_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o cat-debug.elf -T $(USER_LINKER_SCRIPT) $(CAT_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S cat-debug.elf > cat.asm
	$(RISCV_TOOL_PREFIX)nm cat-debug.elf > cat.sym
	$(RISCV_TOOL_PREFIX)strip -s cat-debug.elf -o cat.elf

user-programs: hello.elf spawn_demo.elf shell.elf ls.elf mkdir.elf rmdir.elf mknod.elf env_demo.elf cat.elf

# Check user programs for PIE compatibility
check-pie: $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS)
	@echo "=== Checking user program object files for PIE compatibility ==="
	@FAILED=0; \
	for obj in $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS); do \
		if ! ./check_pie.sh $$obj > /dev/null 2>&1; then \
			./check_pie.sh $$obj; \
			FAILED=1; \
		fi; \
	done; \
	if [ $$FAILED -eq 0 ]; then \
		echo "All user program object files are PIE-compatible."; \
	else \
		exit 1; \
	fi

# ====================================
# Native fstool utility
# ====================================

# fstool compiles kernel filesystem source files natively (with NATIVE_BUILD)
FSTOOL_SRCS = fstool/fstool.c fstool/native_block.c kernel/inode.c \
              kernel/dir.c kernel/file.c kernel/fs.c kernel/device.c

FSTOOL_OBJS = $(patsubst %.c,%.native.o,$(FSTOOL_SRCS))

FSTOOL_HEADERS = kernel/fs.h kernel/fs_types.h kernel/block.h kernel/inode.h \
                 kernel/dir.h kernel/file.h fstool/native_block.h

fstool/%.native.o: fstool/%.c $(FSTOOL_HEADERS)
	$(NATIVE_CC) -DNATIVE_BUILD -Wall -Wextra -O2 -iquote kernel -iquote fstool -c $< -o $@

kernel/%.native.o: kernel/%.c $(FSTOOL_HEADERS)
	$(NATIVE_CC) -DNATIVE_BUILD -Wall -Wextra -O2 -iquote kernel -iquote fstool -c $< -o $@

FSTOOL_BIN = fstool/fstool

$(FSTOOL_BIN): $(FSTOOL_OBJS)
	$(NATIVE_CC) -Wall -Wextra -O2 -o $(FSTOOL_BIN) $(FSTOOL_OBJS)

# ====================================
# Filesystem image
# ====================================

fs-image: $(FSTOOL_BIN) kernel.bin hello.elf spawn_demo.elf shell.elf ls.elf mkdir.elf rmdir.elf mknod.elf env_demo.elf cat.elf
	$(FSTOOL_BIN) format block_storage.bin 256
	$(FSTOOL_BIN) mkdir block_storage.bin /boot
	$(FSTOOL_BIN) mkdir block_storage.bin /bin
	$(FSTOOL_BIN) mkdir block_storage.bin /etc
	$(FSTOOL_BIN) add block_storage.bin /boot/kernel kernel.bin
	$(FSTOOL_BIN) add block_storage.bin /bin/hello hello.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/spawn_demo spawn_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/sh shell.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ls ls.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/mkdir mkdir.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/rmdir rmdir.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/mknod mknod.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/env_demo env_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/cat cat.elf
	$(FSTOOL_BIN) add block_storage.bin /etc/hello.txt hello.txt
	@echo "Filesystem image created."

# ====================================
# QEMU support (compiles kernel for RV64)
# ====================================

QEMU_EXECUTABLE = fs

QEMU_KERNEL_OBJS = $(KERNEL_OBJS)

compile-for-qemu: RISCV_ISA=rv64g
compile-for-qemu: RISCV_ABI=lp64
compile-for-qemu: CFLAGS+=-DQEMU20180
compile-for-qemu: clean-kernel $(QEMU_EXECUTABLE)

$(QEMU_EXECUTABLE): $(QEMU_KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(QEMU_EXECUTABLE) -Map $(QEMU_EXECUTABLE).map -T kernel/qemu.lds $(QEMU_KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(QEMU_EXECUTABLE) > $(QEMU_EXECUTABLE).asm
	$(RISCV_TOOL_PREFIX)nm $(QEMU_EXECUTABLE) > $(QEMU_EXECUTABLE).sym

qemu: compile-for-qemu
	$(QEMU_APP) -machine virt -kernel $(QEMU_EXECUTABLE) -bios none -serial stdio

qemu-gdb: compile-for-qemu
	$(QEMU_APP) -s -S -machine virt -kernel $(QEMU_EXECUTABLE) -bios none -serial stdio

# ====================================
# Build everything
# ====================================

all: boot-rom.txt fs-image

# ====================================
# Clean targets
# ====================================

clean-boot:
	rm -f $(BOOT_OBJS) bootloader bootloader.map bootloader.sym bootloader.asm
	rm -f boot-rom boot-rom.txt

clean-kernel:
	rm -f $(KERNEL_OBJS) kernel-debug.elf kernel.bin kernel.map kernel.sym kernel.asm

clean-user:
	rm -f $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS)
	rm -f hello.elf hello-debug.elf hello.asm hello.sym
	rm -f spawn_demo.elf spawn_demo-debug.elf spawn_demo.asm spawn_demo.sym
	rm -f shell.elf shell-debug.elf shell.asm shell.sym
	rm -f ls.elf ls-debug.elf ls.asm ls.sym
	rm -f mkdir.elf mkdir-debug.elf mkdir.asm mkdir.sym
	rm -f rmdir.elf rmdir-debug.elf rmdir.asm rmdir.sym
	rm -f mknod.elf mknod-debug.elf mknod.asm mknod.sym
	rm -f env_demo.elf env_demo-debug.elf env_demo.asm env_demo.sym
	rm -f cat.elf cat-debug.elf cat.asm cat.sym

clean-fstool:
	rm -f $(FSTOOL_OBJS) $(FSTOOL_BIN)

clean: clean-boot clean-kernel

clean-all: clean clean-user clean-fstool
	rm -f fs fs.map fs.sym fs.asm fs-rom fs-rom.txt

.PHONY: all clean clean-all clean-boot clean-kernel clean-user clean-fstool \
        fs-image fstool user-programs check-pie compile-for-qemu qemu qemu-gdb
