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
# To set up a computer for LogOS (macOS, Ubuntu or Debian, Windows with WSL 2): ./setup.sh
#
#   make all        bootloader ROM image and a fresh disk image (block_storage.bin)
#   make run        LogOS in Logisim's text mode, the fast way (Ctrl-C stops it)
#   make gui        computer.circ in Logisim's window
#   make logosfs    the FUSE driver that mounts block_storage.bin on your computer

# ====================================
# Common configuration
# ====================================

# The RISC-V cross-compiler, found by itself. Another one: make RISCV_TOOL_PREFIX=/its/path/riscv64-elf-
ifndef RISCV_TOOL_PREFIX
  ifneq ($(shell command -v /opt/homebrew/bin/riscv64-elf-gcc),)
    # Homebrew on a Mac with Apple silicon
    RISCV_TOOL_PREFIX := /opt/homebrew/bin/riscv64-elf-
  else ifneq ($(shell command -v riscv64-elf-gcc),)
    # Homebrew on an Intel Mac, Arch Linux
    RISCV_TOOL_PREFIX := riscv64-elf-
  else
    # Ubuntu, Debian, Windows (Ubuntu on WSL): gcc-riscv64-unknown-elf
    RISCV_TOOL_PREFIX := riscv64-unknown-elf-
  endif
endif
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

# Rm program
RM_OBJS = $(USER_COMMON_OBJS) user/rm.user.o

rm.elf: $(RM_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o rm-debug.elf -T $(USER_LINKER_SCRIPT) $(RM_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S rm-debug.elf > rm.asm
	$(RISCV_TOOL_PREFIX)nm rm-debug.elf > rm.sym
	$(RISCV_TOOL_PREFIX)strip -s rm-debug.elf -o rm.elf

# Mv program
MV_OBJS = $(USER_COMMON_OBJS) user/mv.user.o

mv.elf: $(MV_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o mv-debug.elf -T $(USER_LINKER_SCRIPT) $(MV_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S mv-debug.elf > mv.asm
	$(RISCV_TOOL_PREFIX)nm mv-debug.elf > mv.sym
	$(RISCV_TOOL_PREFIX)strip -s mv-debug.elf -o mv.elf

# Ln program
LN_OBJS = $(USER_COMMON_OBJS) user/ln.user.o

ln.elf: $(LN_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o ln-debug.elf -T $(USER_LINKER_SCRIPT) $(LN_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S ln-debug.elf > ln.asm
	$(RISCV_TOOL_PREFIX)nm ln-debug.elf > ln.sym
	$(RISCV_TOOL_PREFIX)strip -s ln-debug.elf -o ln.elf

# Cp program
CP_OBJS = $(USER_COMMON_OBJS) user/cp.user.o

cp.elf: $(CP_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o cp-debug.elf -T $(USER_LINKER_SCRIPT) $(CP_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S cp-debug.elf > cp.asm
	$(RISCV_TOOL_PREFIX)nm cp-debug.elf > cp.sym
	$(RISCV_TOOL_PREFIX)strip -s cp-debug.elf -o cp.elf

# Ed program (line editor)
ED_OBJS = $(USER_COMMON_OBJS) user/ed.user.o

ed.elf: $(ED_OBJS)
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o ed-debug.elf -T $(USER_LINKER_SCRIPT) $(ED_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S ed-debug.elf > ed.asm
	$(RISCV_TOOL_PREFIX)nm ed-debug.elf > ed.sym
	$(RISCV_TOOL_PREFIX)strip -s ed-debug.elf -o ed.elf

user-programs: hello.elf spawn_demo.elf shell.elf ls.elf mkdir.elf rmdir.elf mknod.elf env_demo.elf cat.elf rm.elf mv.elf ln.elf cp.elf ed.elf

# Check user programs for PIE compatibility
check-pie: $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS) $(RM_OBJS) $(MV_OBJS) $(LN_OBJS) $(CP_OBJS) $(ED_OBJS)
	@echo "=== Checking user program object files for PIE compatibility ==="
	@FAILED=0; \
	for obj in $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS) $(RM_OBJS) $(MV_OBJS) $(LN_OBJS) $(CP_OBJS) $(ED_OBJS); do \
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
FSTOOL_SRCS = fstool/fstool.c fstool/native_block.c kernel/block.c kernel/inode.c \
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

# logosfs: a LogOS disk image mounted on the host through FUSE, with the same kernel
# file system code as fstool. Not part of "all": it needs FUSE (macFUSE on macOS,
# libfuse3 and its headers on Linux). See README.md.
LOGOSFS_BIN = fstool/logosfs
LOGOSFS_OBJS = fstool/logosfs.native.o $(filter-out fstool/fstool.native.o,$(FSTOOL_OBJS))
# libfuse3's flags from pkg-config or, without it, where macFUSE (/usr/local) and
# Linux (/usr) put libfuse3; without the fuse3 directory, <fuse.h> is macFUSE's FUSE 2
FUSE_PKG = PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$$PKG_CONFIG_PATH pkg-config fuse3
FUSE_CFLAGS = $(shell $(FUSE_PKG) --cflags 2>/dev/null || echo -I/usr/local/include/fuse3 -I/usr/include/fuse3)
FUSE_LIBS = $(shell $(FUSE_PKG) --libs 2>/dev/null || echo -L/usr/local/lib -lfuse3 -lpthread)

fstool/logosfs.native.o: fstool/logosfs.c $(FSTOOL_HEADERS)
	$(NATIVE_CC) -DNATIVE_BUILD -DFUSE_DARWIN_ENABLE_EXTENSIONS=0 -D_FILE_OFFSET_BITS=64 $(FUSE_CFLAGS) \
		-Wall -Wextra -O2 -iquote kernel -iquote fstool -c $< -o $@

$(LOGOSFS_BIN): $(LOGOSFS_OBJS)
	$(NATIVE_CC) -Wall -Wextra -O2 -o $(LOGOSFS_BIN) $(LOGOSFS_OBJS) $(FUSE_LIBS)

logosfs: $(LOGOSFS_BIN)

# ====================================
# Filesystem image
# ====================================

fs-image: $(FSTOOL_BIN) kernel.bin hello.elf spawn_demo.elf shell.elf ls.elf mkdir.elf rmdir.elf mknod.elf env_demo.elf cat.elf rm.elf mv.elf ln.elf cp.elf ed.elf
	$(FSTOOL_BIN) format block_storage.bin 512
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
	$(FSTOOL_BIN) add block_storage.bin /bin/rm rm.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/mv mv.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ln ln.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/cp cp.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/ed ed.elf
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
# LogOS in Logisim
# ====================================

# Logisim Evolution UCD, fetched from its releases (make logisim; setup.sh does it).
# Another one: make run LOGISIM_JAR=/its/path/logisim-evolution-ucd-5.0.6-all.jar
LOGISIM_VERSION = 5.0.6
LOGISIM_JAR = logisim-evolution-ucd-$(LOGISIM_VERSION)-all.jar
LOGISIM_URL = https://github.com/pavelgladyshev/logisim-evolution-ucd/releases/download/v$(LOGISIM_VERSION)/logisim-evolution-ucd-$(LOGISIM_VERSION)-all.jar

logisim: $(LOGISIM_JAR)

$(LOGISIM_JAR):
	curl -fL --progress-bar -o $@.part $(LOGISIM_URL) && mv $@.part $@

# Text mode: the TTY is this terminal, and the simulation runs much faster than in the window.
# The disk image is made only if there is none (make fs-image makes a fresh one, without the
# files you added since)
run: $(LOGISIM_JAR) block_storage.bin
	java -jar $(LOGISIM_JAR) computer.circ --tty raw

block_storage.bin:
	$(MAKE) fs-image

# The window. On Windows (WSL), Java for Windows, when it is on the PATH, gives a native window
# and the FPGA board's USB; without it, Linux's Java shows the window through WSLg
gui: $(LOGISIM_JAR)
	$(if $(shell command -v javaw.exe),javaw.exe,java) -jar $(LOGISIM_JAR) computer.circ

# make print-VARIABLE: a variable's value (setup.sh asks for RISCV_TOOL_PREFIX and LOGISIM_JAR)
print-%:
	@echo '$($*)'

# ====================================
# Clean targets
# ====================================

clean-boot:
	rm -f $(BOOT_OBJS) bootloader bootloader.map bootloader.sym bootloader.asm
	rm -f boot-rom boot-rom.txt

clean-kernel:
	rm -f $(KERNEL_OBJS) kernel-debug.elf kernel.bin kernel.map kernel.sym kernel.asm

clean-user:
	rm -f $(HELLO_OBJS) $(SPAWN_DEMO_OBJS) $(SHELL_OBJS) $(LS_OBJS) $(MKDIR_OBJS) $(RMDIR_OBJS) $(MKNOD_OBJS) $(ENV_DEMO_OBJS) $(RM_OBJS) $(MV_OBJS) $(LN_OBJS) $(CP_OBJS) $(ED_OBJS)
	rm -f hello.elf hello-debug.elf hello.asm hello.sym
	rm -f spawn_demo.elf spawn_demo-debug.elf spawn_demo.asm spawn_demo.sym
	rm -f shell.elf shell-debug.elf shell.asm shell.sym
	rm -f ls.elf ls-debug.elf ls.asm ls.sym
	rm -f mkdir.elf mkdir-debug.elf mkdir.asm mkdir.sym
	rm -f rmdir.elf rmdir-debug.elf rmdir.asm rmdir.sym
	rm -f mknod.elf mknod-debug.elf mknod.asm mknod.sym
	rm -f env_demo.elf env_demo-debug.elf env_demo.asm env_demo.sym
	rm -f cat.elf cat-debug.elf cat.asm cat.sym
	rm -f rm.elf rm-debug.elf rm.asm rm.sym
	rm -f mv.elf mv-debug.elf mv.asm mv.sym
	rm -f ln.elf ln-debug.elf ln.asm ln.sym
	rm -f cp.elf cp-debug.elf cp.asm cp.sym
	rm -f ed.elf ed-debug.elf ed.asm ed.sym

clean-fstool:
	rm -f $(FSTOOL_OBJS) $(FSTOOL_BIN) fstool/logosfs.native.o $(LOGOSFS_BIN)

clean: clean-boot clean-kernel

clean-all: clean clean-user clean-fstool
	rm -f fs fs.map fs.sym fs.asm fs-rom fs-rom.txt

.PHONY: all clean clean-all clean-boot clean-kernel clean-user clean-fstool logosfs \
        fs-image fstool user-programs check-pie compile-for-qemu qemu qemu-gdb logisim run gui
