# Makefile for RISC-V Kernel with Bootloader
# Logisim RISC-V Computer System model by Pavel Gladyshev
# Licensed under Creative Commons Attribution International License 4.0
#
# Directory structure:
#   kernel/  - Kernel source files and headers
#   boot/    - Bootloader source files
#   user/    - User programs and runtime library
#   fstool/  - Native filesystem tool for host
#   build/   - Build output (ELFs, binaries, ROM images, filesystem image)
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
BUILD_DIR = build

# Common RISC-V compilation flags
RISCV_CFLAGS = -O -march=$(RISCV_ISA) -mabi=$(RISCV_ABI) -mcmodel=medany -g $(CFLAGS)

# Default target: build bootloader ROM image
.DEFAULT_GOAL := boot-rom.txt

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

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

$(BUILD_DIR)/bootloader: $(BOOT_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/bootloader -Map $(BUILD_DIR)/bootloader.map -T boot/boot.lds $(BOOT_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/bootloader > $(BUILD_DIR)/bootloader.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/bootloader > $(BUILD_DIR)/bootloader.sym

boot-rom: $(BUILD_DIR)/bootloader
	$(RISCV_TOOL_PREFIX)objcopy --only-section .init --only-section .text --only-section .rodata \
		--only-section .srodata --only-section .data --only-section .sdata \
		--only-section .text.* --only-section .rodata.* --only-section .srodata.* \
		--only-section .data.* --only-section .sdata.* \
		--output-target binary $(BUILD_DIR)/bootloader boot-rom

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
              kernel/pipe.o kernel/shm.o kernel/sem.o kernel/syscall.o kernel/main.o

KERNEL_HEADERS = kernel/fs.h kernel/types.h kernel/fs_types.h kernel/string.h \
                 kernel/console.h kernel/block.h kernel/inode.h kernel/dir.h \
                 kernel/file.h kernel/device.h kernel/console_dev.h kernel/elf.h \
                 kernel/loader.h kernel/trap.h kernel/syscall.h kernel/process.h \
                 kernel/pipe.h kernel/shm.h kernel/sem.h

kernel/%.o: kernel/%.c $(KERNEL_HEADERS)
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

kernel/%.o: kernel/%.S $(KERNEL_HEADERS)
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

$(BUILD_DIR)/kernel-debug.elf: $(KERNEL_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/kernel-debug.elf -Map $(BUILD_DIR)/kernel.map -T kernel/kernel.lds $(KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/kernel-debug.elf > $(BUILD_DIR)/kernel.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/kernel-debug.elf > $(BUILD_DIR)/kernel.sym

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel-debug.elf
	$(RISCV_TOOL_PREFIX)objcopy --output-target binary $(BUILD_DIR)/kernel-debug.elf $(BUILD_DIR)/kernel.bin

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

# ---- User program list ----
# To add a new program, just append its name here.
# The source file must be user/<name>.c
USER_PROGS = hello spawn_demo shell ls mkdir rmdir mknod env_demo cat \
             fork_demo pipe_demo pipe_test redir_test fd_test \
             rm mv ln cp ps kill ed shm_test sem_test \
             fs_test proc_test stress_test link_test script_test

# Derive ELF list and object file list from USER_PROGS
USER_ELFS = $(patsubst %,$(BUILD_DIR)/%.elf,$(USER_PROGS))
USER_PROG_OBJS = $(patsubst %,user/%.user.o,$(USER_PROGS))

# Pattern rule: build any user program ELF from its .user.o + common objects
$(BUILD_DIR)/%.elf: user/%.user.o $(USER_COMMON_OBJS) $(USER_LINKER_SCRIPT) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/$*-debug.elf -T $(USER_LINKER_SCRIPT) $(USER_COMMON_OBJS) $<
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/$*-debug.elf > $(BUILD_DIR)/$*.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/$*-debug.elf > $(BUILD_DIR)/$*.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/$*-debug.elf -o $@

user-programs: $(USER_ELFS)

# Check user programs for PIE compatibility
check-pie: $(USER_PROG_OBJS) $(USER_COMMON_OBJS)
	@echo "=== Checking user program object files for PIE compatibility ==="
	@FAILED=0; \
	for obj in $(USER_PROG_OBJS) $(USER_COMMON_OBJS); do \
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

# Map program names to filesystem paths (shell.elf -> /bin/sh, others -> /bin/<name>)
SHELL_FS_NAME = sh

fs-image: $(FSTOOL_BIN) $(BUILD_DIR)/kernel.bin $(USER_ELFS) | $(BUILD_DIR)
	$(FSTOOL_BIN) format block_storage.bin 1024
	$(FSTOOL_BIN) mkdir block_storage.bin /boot
	$(FSTOOL_BIN) mkdir block_storage.bin /bin
	$(FSTOOL_BIN) mkdir block_storage.bin /etc
	$(FSTOOL_BIN) add block_storage.bin /boot/kernel $(BUILD_DIR)/kernel.bin
	@for prog in $(USER_PROGS); do \
		if [ "$$prog" = "shell" ]; then \
			$(FSTOOL_BIN) add block_storage.bin /bin/$(SHELL_FS_NAME) $(BUILD_DIR)/$$prog.elf; \
		else \
			$(FSTOOL_BIN) add block_storage.bin /bin/$$prog $(BUILD_DIR)/$$prog.elf; \
		fi; \
	done
	$(FSTOOL_BIN) add block_storage.bin /etc/hello.txt hello.txt
	$(FSTOOL_BIN) add block_storage.bin /etc/rc rc
	$(FSTOOL_BIN) add block_storage.bin /etc/test.sh test.sh
	@echo "Filesystem image created."

# ====================================
# QEMU support (compiles kernel for RV64)
# ====================================

QEMU_EXECUTABLE = $(BUILD_DIR)/fs

QEMU_KERNEL_OBJS = $(KERNEL_OBJS)

compile-for-qemu: RISCV_ISA=rv64g
compile-for-qemu: RISCV_ABI=lp64
compile-for-qemu: CFLAGS+=-DQEMU20180
compile-for-qemu: clean-kernel $(QEMU_EXECUTABLE)

$(QEMU_EXECUTABLE): $(QEMU_KERNEL_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(QEMU_EXECUTABLE) -Map $(BUILD_DIR)/fs.map -T kernel/qemu.lds $(QEMU_KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(QEMU_EXECUTABLE) > $(BUILD_DIR)/fs.asm
	$(RISCV_TOOL_PREFIX)nm $(QEMU_EXECUTABLE) > $(BUILD_DIR)/fs.sym

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
	rm -f $(BOOT_OBJS)

clean-kernel:
	rm -f $(KERNEL_OBJS)

clean-user:
	rm -f $(USER_PROG_OBJS) $(USER_COMMON_OBJS)

clean-fstool:
	rm -f $(FSTOOL_OBJS) $(FSTOOL_BIN)

clean: clean-boot clean-kernel

clean-all: clean clean-user clean-fstool
	rm -rf $(BUILD_DIR)
	rm -f boot-rom boot-rom.txt block_storage.bin

.PHONY: all clean clean-all clean-boot clean-kernel clean-user clean-fstool \
        fs-image fstool user-programs check-pie compile-for-qemu qemu qemu-gdb
