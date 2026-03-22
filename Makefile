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
              kernel/pipe.o kernel/syscall.o kernel/main.o

KERNEL_HEADERS = kernel/fs.h kernel/types.h kernel/fs_types.h kernel/string.h \
                 kernel/console.h kernel/block.h kernel/inode.h kernel/dir.h \
                 kernel/file.h kernel/device.h kernel/console_dev.h kernel/elf.h \
                 kernel/loader.h kernel/trap.h kernel/syscall.h kernel/process.h \
                 kernel/pipe.h

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

# Hello program
HELLO_OBJS = $(USER_COMMON_OBJS) user/hello.user.o

$(BUILD_DIR)/hello.elf: $(HELLO_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/hello-debug.elf -T $(USER_LINKER_SCRIPT) $(HELLO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/hello-debug.elf > $(BUILD_DIR)/hello.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/hello-debug.elf > $(BUILD_DIR)/hello.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/hello-debug.elf -o $(BUILD_DIR)/hello.elf

# Spawn demo program
SPAWN_DEMO_OBJS = $(USER_COMMON_OBJS) user/spawn_demo.user.o

$(BUILD_DIR)/spawn_demo.elf: $(SPAWN_DEMO_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/spawn_demo-debug.elf -T $(USER_LINKER_SCRIPT) $(SPAWN_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/spawn_demo-debug.elf > $(BUILD_DIR)/spawn_demo.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/spawn_demo-debug.elf > $(BUILD_DIR)/spawn_demo.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/spawn_demo-debug.elf -o $(BUILD_DIR)/spawn_demo.elf

# Shell program
SHELL_OBJS = $(USER_COMMON_OBJS) user/shell.user.o

$(BUILD_DIR)/shell.elf: $(SHELL_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/shell-debug.elf -T $(USER_LINKER_SCRIPT) $(SHELL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/shell-debug.elf > $(BUILD_DIR)/shell.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/shell-debug.elf > $(BUILD_DIR)/shell.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/shell-debug.elf -o $(BUILD_DIR)/shell.elf

# Ls program
LS_OBJS = $(USER_COMMON_OBJS) user/ls.user.o

$(BUILD_DIR)/ls.elf: $(LS_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/ls-debug.elf -T $(USER_LINKER_SCRIPT) $(LS_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/ls-debug.elf > $(BUILD_DIR)/ls.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/ls-debug.elf > $(BUILD_DIR)/ls.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/ls-debug.elf -o $(BUILD_DIR)/ls.elf

# Mkdir program
MKDIR_OBJS = $(USER_COMMON_OBJS) user/mkdir.user.o

$(BUILD_DIR)/mkdir.elf: $(MKDIR_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/mkdir-debug.elf -T $(USER_LINKER_SCRIPT) $(MKDIR_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/mkdir-debug.elf > $(BUILD_DIR)/mkdir.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/mkdir-debug.elf > $(BUILD_DIR)/mkdir.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/mkdir-debug.elf -o $(BUILD_DIR)/mkdir.elf

# Rmdir program
RMDIR_OBJS = $(USER_COMMON_OBJS) user/rmdir.user.o

$(BUILD_DIR)/rmdir.elf: $(RMDIR_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/rmdir-debug.elf -T $(USER_LINKER_SCRIPT) $(RMDIR_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/rmdir-debug.elf > $(BUILD_DIR)/rmdir.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/rmdir-debug.elf > $(BUILD_DIR)/rmdir.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/rmdir-debug.elf -o $(BUILD_DIR)/rmdir.elf

# Mknod program
MKNOD_OBJS = $(USER_COMMON_OBJS) user/mknod.user.o

$(BUILD_DIR)/mknod.elf: $(MKNOD_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/mknod-debug.elf -T $(USER_LINKER_SCRIPT) $(MKNOD_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/mknod-debug.elf > $(BUILD_DIR)/mknod.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/mknod-debug.elf > $(BUILD_DIR)/mknod.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/mknod-debug.elf -o $(BUILD_DIR)/mknod.elf

# Env demo program
ENV_DEMO_OBJS = $(USER_COMMON_OBJS) user/env_demo.user.o

$(BUILD_DIR)/env_demo.elf: $(ENV_DEMO_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/env_demo-debug.elf -T $(USER_LINKER_SCRIPT) $(ENV_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/env_demo-debug.elf > $(BUILD_DIR)/env_demo.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/env_demo-debug.elf > $(BUILD_DIR)/env_demo.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/env_demo-debug.elf -o $(BUILD_DIR)/env_demo.elf

# Cat program
CAT_OBJS = $(USER_COMMON_OBJS) user/cat.user.o

$(BUILD_DIR)/cat.elf: $(CAT_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/cat-debug.elf -T $(USER_LINKER_SCRIPT) $(CAT_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/cat-debug.elf > $(BUILD_DIR)/cat.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/cat-debug.elf > $(BUILD_DIR)/cat.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/cat-debug.elf -o $(BUILD_DIR)/cat.elf

# Fork demo program
FORK_DEMO_OBJS = $(USER_COMMON_OBJS) user/fork_demo.user.o

$(BUILD_DIR)/fork_demo.elf: $(FORK_DEMO_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/fork_demo-debug.elf -T $(USER_LINKER_SCRIPT) $(FORK_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/fork_demo-debug.elf > $(BUILD_DIR)/fork_demo.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/fork_demo-debug.elf > $(BUILD_DIR)/fork_demo.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/fork_demo-debug.elf -o $(BUILD_DIR)/fork_demo.elf

# Pipe demo program
PIPE_DEMO_OBJS = $(USER_COMMON_OBJS) user/pipe_demo.user.o

$(BUILD_DIR)/pipe_demo.elf: $(PIPE_DEMO_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/pipe_demo-debug.elf -T $(USER_LINKER_SCRIPT) $(PIPE_DEMO_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/pipe_demo-debug.elf > $(BUILD_DIR)/pipe_demo.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/pipe_demo-debug.elf > $(BUILD_DIR)/pipe_demo.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/pipe_demo-debug.elf -o $(BUILD_DIR)/pipe_demo.elf

# Pipe test program
PIPE_TEST_OBJS = $(USER_COMMON_OBJS) user/pipe_test.user.o

$(BUILD_DIR)/pipe_test.elf: $(PIPE_TEST_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/pipe_test-debug.elf -T $(USER_LINKER_SCRIPT) $(PIPE_TEST_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/pipe_test-debug.elf > $(BUILD_DIR)/pipe_test.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/pipe_test-debug.elf > $(BUILD_DIR)/pipe_test.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/pipe_test-debug.elf -o $(BUILD_DIR)/pipe_test.elf

# Redirection test program
REDIR_TEST_OBJS = $(USER_COMMON_OBJS) user/redir_test.user.o

$(BUILD_DIR)/redir_test.elf: $(REDIR_TEST_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/redir_test-debug.elf -T $(USER_LINKER_SCRIPT) $(REDIR_TEST_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/redir_test-debug.elf > $(BUILD_DIR)/redir_test.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/redir_test-debug.elf > $(BUILD_DIR)/redir_test.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/redir_test-debug.elf -o $(BUILD_DIR)/redir_test.elf

# File descriptor test program
FD_TEST_OBJS = $(USER_COMMON_OBJS) user/fd_test.user.o

$(BUILD_DIR)/fd_test.elf: $(FD_TEST_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/fd_test-debug.elf -T $(USER_LINKER_SCRIPT) $(FD_TEST_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/fd_test-debug.elf > $(BUILD_DIR)/fd_test.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/fd_test-debug.elf > $(BUILD_DIR)/fd_test.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/fd_test-debug.elf -o $(BUILD_DIR)/fd_test.elf

# Rm program
RM_OBJS = $(USER_COMMON_OBJS) user/rm.user.o

$(BUILD_DIR)/rm.elf: $(RM_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/rm-debug.elf -T $(USER_LINKER_SCRIPT) $(RM_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/rm-debug.elf > $(BUILD_DIR)/rm.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/rm-debug.elf > $(BUILD_DIR)/rm.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/rm-debug.elf -o $(BUILD_DIR)/rm.elf

# Mv program
MV_OBJS = $(USER_COMMON_OBJS) user/mv.user.o

$(BUILD_DIR)/mv.elf: $(MV_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/mv-debug.elf -T $(USER_LINKER_SCRIPT) $(MV_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/mv-debug.elf > $(BUILD_DIR)/mv.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/mv-debug.elf > $(BUILD_DIR)/mv.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/mv-debug.elf -o $(BUILD_DIR)/mv.elf

# Ln program
LN_OBJS = $(USER_COMMON_OBJS) user/ln.user.o

$(BUILD_DIR)/ln.elf: $(LN_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/ln-debug.elf -T $(USER_LINKER_SCRIPT) $(LN_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/ln-debug.elf > $(BUILD_DIR)/ln.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/ln-debug.elf > $(BUILD_DIR)/ln.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/ln-debug.elf -o $(BUILD_DIR)/ln.elf

# Cp program
CP_OBJS = $(USER_COMMON_OBJS) user/cp.user.o

$(BUILD_DIR)/cp.elf: $(CP_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/cp-debug.elf -T $(USER_LINKER_SCRIPT) $(CP_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/cp-debug.elf > $(BUILD_DIR)/cp.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/cp-debug.elf > $(BUILD_DIR)/cp.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/cp-debug.elf -o $(BUILD_DIR)/cp.elf

# Ps program
PS_OBJS = $(USER_COMMON_OBJS) user/ps.user.o

$(BUILD_DIR)/ps.elf: $(PS_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/ps-debug.elf -T $(USER_LINKER_SCRIPT) $(PS_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/ps-debug.elf > $(BUILD_DIR)/ps.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/ps-debug.elf > $(BUILD_DIR)/ps.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/ps-debug.elf -o $(BUILD_DIR)/ps.elf

# Kill program
KILL_OBJS = $(USER_COMMON_OBJS) user/kill.user.o

$(BUILD_DIR)/kill.elf: $(KILL_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/kill-debug.elf -T $(USER_LINKER_SCRIPT) $(KILL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/kill-debug.elf > $(BUILD_DIR)/kill.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/kill-debug.elf > $(BUILD_DIR)/kill.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/kill-debug.elf -o $(BUILD_DIR)/kill.elf

# Ed program (line editor)
ED_OBJS = $(USER_COMMON_OBJS) user/ed.user.o

$(BUILD_DIR)/ed.elf: $(ED_OBJS) | $(BUILD_DIR)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $(BUILD_DIR)/ed-debug.elf -T $(USER_LINKER_SCRIPT) $(ED_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $(BUILD_DIR)/ed-debug.elf > $(BUILD_DIR)/ed.asm
	$(RISCV_TOOL_PREFIX)nm $(BUILD_DIR)/ed-debug.elf > $(BUILD_DIR)/ed.sym
	$(RISCV_TOOL_PREFIX)strip -s $(BUILD_DIR)/ed-debug.elf -o $(BUILD_DIR)/ed.elf

USER_ELFS = $(BUILD_DIR)/hello.elf $(BUILD_DIR)/spawn_demo.elf $(BUILD_DIR)/shell.elf \
            $(BUILD_DIR)/ls.elf $(BUILD_DIR)/mkdir.elf $(BUILD_DIR)/rmdir.elf \
            $(BUILD_DIR)/mknod.elf $(BUILD_DIR)/env_demo.elf $(BUILD_DIR)/cat.elf \
            $(BUILD_DIR)/fork_demo.elf $(BUILD_DIR)/pipe_demo.elf $(BUILD_DIR)/pipe_test.elf \
            $(BUILD_DIR)/redir_test.elf $(BUILD_DIR)/fd_test.elf \
            $(BUILD_DIR)/rm.elf $(BUILD_DIR)/mv.elf $(BUILD_DIR)/ln.elf $(BUILD_DIR)/cp.elf \
            $(BUILD_DIR)/ps.elf $(BUILD_DIR)/kill.elf $(BUILD_DIR)/ed.elf

user-programs: $(USER_ELFS)

# Check user programs for PIE compatibility
check-pie: $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS) $(FORK_DEMO_OBJS) $(PIPE_DEMO_OBJS) $(PIPE_TEST_OBJS) $(REDIR_TEST_OBJS) $(FD_TEST_OBJS) $(RM_OBJS) $(MV_OBJS) $(LN_OBJS) $(CP_OBJS) $(PS_OBJS) $(KILL_OBJS) $(ED_OBJS)
	@echo "=== Checking user program object files for PIE compatibility ==="
	@FAILED=0; \
	for obj in $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS) $(FORK_DEMO_OBJS) $(PIPE_DEMO_OBJS) $(PIPE_TEST_OBJS) $(REDIR_TEST_OBJS) $(FD_TEST_OBJS) $(RM_OBJS) $(MV_OBJS) $(LN_OBJS) $(CP_OBJS) $(PS_OBJS) $(KILL_OBJS) $(ED_OBJS); do \
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

fs-image: $(FSTOOL_BIN) $(BUILD_DIR)/kernel.bin $(USER_ELFS) | $(BUILD_DIR)
	$(FSTOOL_BIN) format block_storage.bin 1024
	$(FSTOOL_BIN) mkdir block_storage.bin /boot
	$(FSTOOL_BIN) mkdir block_storage.bin /bin
	$(FSTOOL_BIN) mkdir block_storage.bin /etc
	$(FSTOOL_BIN) add block_storage.bin /boot/kernel $(BUILD_DIR)/kernel.bin
	$(FSTOOL_BIN) add block_storage.bin /bin/hello $(BUILD_DIR)/hello.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/spawn_demo $(BUILD_DIR)/spawn_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/sh $(BUILD_DIR)/shell.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ls $(BUILD_DIR)/ls.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/mkdir $(BUILD_DIR)/mkdir.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/rmdir $(BUILD_DIR)/rmdir.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/mknod $(BUILD_DIR)/mknod.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/env_demo $(BUILD_DIR)/env_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/cat $(BUILD_DIR)/cat.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/fork_demo $(BUILD_DIR)/fork_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/pipe_demo $(BUILD_DIR)/pipe_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/pipe_test $(BUILD_DIR)/pipe_test.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/redir_test $(BUILD_DIR)/redir_test.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/fd_test $(BUILD_DIR)/fd_test.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/rm $(BUILD_DIR)/rm.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/mv $(BUILD_DIR)/mv.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ln $(BUILD_DIR)/ln.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/cp $(BUILD_DIR)/cp.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ps $(BUILD_DIR)/ps.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/kill $(BUILD_DIR)/kill.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ed $(BUILD_DIR)/ed.elf
	$(FSTOOL_BIN) add block_storage.bin /etc/hello.txt hello.txt
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
	rm -f $(foreach prog,$(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS) $(CAT_OBJS) $(FORK_DEMO_OBJS) $(PIPE_DEMO_OBJS) $(PIPE_TEST_OBJS) $(REDIR_TEST_OBJS) $(FD_TEST_OBJS) $(RM_OBJS) $(MV_OBJS) $(LN_OBJS) $(CP_OBJS) $(PS_OBJS) $(KILL_OBJS) $(ED_OBJS),$(prog))

clean-fstool:
	rm -f $(FSTOOL_OBJS) $(FSTOOL_BIN)

clean: clean-boot clean-kernel

clean-all: clean clean-user clean-fstool
	rm -rf $(BUILD_DIR)
	rm -f boot-rom boot-rom.txt block_storage.bin

.PHONY: all clean clean-all clean-boot clean-kernel clean-user clean-fstool \
        fs-image fstool user-programs check-pie compile-for-qemu qemu qemu-gdb
