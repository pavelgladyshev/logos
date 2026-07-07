# Makefile for RISC-V Kernel with Bootloader
# Logisim RISC-V Computer System model by Pavel Gladyshev
# Licensed under Creative Commons Attribution International License 4.0
#
# Directory structure:
#   boot/    - Bootloader source files
#   kernel/  - Kernel source files and headers
#   user/    - User programs and runtime library
#   fstool/  - Native filesystem tool for host
#   build/   - All generated build artifacts
#
# To install required packages in Ubuntu Linux:
#   sudo apt install build-essential binutils-riscv64-unknown-elf gcc-riscv64-unknown-elf

# ====================================
# Configuration
# ====================================

RISCV_TOOL_PREFIX = riscv64-none-elf-
RISCV_ISA = rv32im_zicsr
RISCV_ABI = ilp32
CFLAGS = -Wno-builtin-declaration-mismatch
NATIVE_CC = gcc

RISCV_CFLAGS = -O -march=$(RISCV_ISA) -mabi=$(RISCV_ABI) -mcmodel=medany -g $(CFLAGS)

BUILD_LOGISIM = ../../build/logisim

BUILD_ESP32C3 = ../../build/esp32c3

.DEFAULT_GOAL := $(BUILD_LOGISIM)/boot-rom.txt

# Create build directory and subdirectories on demand
$(BUILD_LOGISIM)/:
	mkdir -p $@

$(BUILD_LOGISIM)/boot/ $(BUILD_LOGISIM)/kernel/ $(BUILD_LOGISIM)/user/ $(BUILD_LOGISIM)/fstool/:
	mkdir -p $@

# ====================================
# 1. Bootloader (ROM image for Logisim)
# ====================================

BOOT_OBJS = $(BUILD_LOGISIM)/boot/boot_crt0.o $(BUILD_LOGISIM)/boot/boot_main.o $(BUILD_LOGISIM)/boot/boot_io.o \
            $(BUILD_LOGISIM)/boot/boot_fs.o $(BUILD_LOGISIM)/boot/boot_string.o

BOOT_HEADERS = boot/boot.h kernel/types.h kernel/fs_types.h

$(BUILD_LOGISIM)/boot/%.o: boot/%.c $(BOOT_HEADERS) | $(BUILD_LOGISIM)/boot/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

$(BUILD_LOGISIM)/boot/%.o: boot/%.S $(BOOT_HEADERS) | $(BUILD_LOGISIM)/boot/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

$(BUILD_LOGISIM)/bootloader: $(BOOT_OBJS) | $(BUILD_LOGISIM)/
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $@ -Map $(BUILD_LOGISIM)/bootloader.map -T boot/boot.lds $(BOOT_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $@ > $(BUILD_LOGISIM)/bootloader.asm
	$(RISCV_TOOL_PREFIX)nm $@ > $(BUILD_LOGISIM)/bootloader.sym

$(BUILD_LOGISIM)/boot-rom: $(BUILD_LOGISIM)/bootloader | $(BUILD_LOGISIM)/
	$(RISCV_TOOL_PREFIX)objcopy --only-section .init --only-section .text --only-section .rodata \
		--only-section .srodata --only-section .data --only-section .sdata \
		--only-section .text.* --only-section .rodata.* --only-section .srodata.* \
		--only-section .data.* --only-section .sdata.* \
		--output-target binary $(BUILD_LOGISIM)/bootloader $@

$(BUILD_LOGISIM)/boot-rom.txt: $(BUILD_LOGISIM)/boot-rom | $(BUILD_LOGISIM)/
	echo "v2.0 raw" > $@
	hexdump -v -e '/4 "%08x""\n"""' $(BUILD_LOGISIM)/boot-rom >> $@

# ====================================
# 2. Kernel (binary loaded into RAM by bootloader)
# ====================================

KERNEL_OBJS = $(BUILD_LOGISIM)/kernel/crt0.o $(BUILD_LOGISIM)/kernel/string.o $(BUILD_LOGISIM)/kernel/console.o \
              $(BUILD_LOGISIM)/kernel/block.o $(BUILD_LOGISIM)/kernel/inode.o $(BUILD_LOGISIM)/kernel/dir.o \
              $(BUILD_LOGISIM)/kernel/file.o $(BUILD_LOGISIM)/kernel/fs.o $(BUILD_LOGISIM)/kernel/device.o \
              $(BUILD_LOGISIM)/kernel/console_dev.o $(BUILD_LOGISIM)/kernel/display_dev.o $(BUILD_LOGISIM)/kernel/loader.o \
              $(BUILD_LOGISIM)/kernel/loader_asm.o $(BUILD_LOGISIM)/kernel/trap.o $(BUILD_LOGISIM)/kernel/process.o \
              $(BUILD_LOGISIM)/kernel/syscall.o $(BUILD_LOGISIM)/kernel/main.o

KERNEL_HEADERS = kernel/fs.h kernel/types.h kernel/fs_types.h kernel/logisim/string.h \
                 kernel/logisim/console.h kernel/logisim/block.h kernel/inode.h kernel/dir.h \
                 kernel/file.h kernel/device.h kernel/logisim/display_dev.h kernel/logisim/console_dev.h kernel/logisim/elf.h \
                 kernel/logisim/loader.h kernel/logisim/trap.h kernel/logisim/syscall.h kernel/process.h

$(BUILD_LOGISIM)/kernel/crt0.o: kernel/logisim/crt0.S $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -Ikernel/logisim -c $< -o $@

#$(BUILD_LOGISIM)/kernel/%.o: kernel/logisim/%.S $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
#	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -Ikernel/logisim -c $< -o $@
#
#$(BUILD_LOGISIM)/kernel/%.o: kernel/%.S $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
#	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -Ikernel/logisim -c $< -o $@

# target-specific assembly (e.g. crt0.S under kernel/<target>/)

$(BUILD_LOGISIM)/kernel/%.o: kernel/logisim/%.c $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -Ikernel/logisim -c $< -o $@

$(BUILD_LOGISIM)/kernel/%.o: kernel/%.c $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -Ikernel/logisim -c $< -o $@

$(BUILD_LOGISIM)/kernel/%.o: kernel/logisim/%.S $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -Ikernel/logisim -c $< -o $@

# shared/common assembly
$(BUILD_LOGISIM)/kernel/%.o: kernel/%.S $(KERNEL_HEADERS) | $(BUILD_LOGISIM)/kernel/
	$(RISCV_TOOL_PREFIX)gcc $(RISCV_CFLAGS) -Ikernel -c $< -o $@

$(BUILD_LOGISIM)/kernel-debug.elf: $(KERNEL_OBJS) | $(BUILD_LOGISIM)/
	$(RISCV_TOOL_PREFIX)ld -nostdlib -o $@ -Map $(BUILD_LOGISIM)/kernel.map -T kernel/kernel.lds $(KERNEL_OBJS)
	$(RISCV_TOOL_PREFIX)objdump -S $@ > $(BUILD_LOGISIM)/kernel.asm
	$(RISCV_TOOL_PREFIX)nm $@ > $(BUILD_LOGISIM)/kernel.sym

$(BUILD_LOGISIM)/kernel.bin: $(BUILD_LOGISIM)/kernel-debug.elf | $(BUILD_LOGISIM)/
	$(RISCV_TOOL_PREFIX)objcopy --output-target binary $< $@

# ====================================
# 3. User programs (PIE executables loaded by kernel)
# ====================================

USER_LINKER_SCRIPT = user/user.lds
USER_CFLAGS = -fPIE -fno-jump-tables $(RISCV_CFLAGS)

USER_LIB_OBJS = $(BUILD_LOGISIM)/user/syscall.user.o $(BUILD_LOGISIM)/user/libc.user.o
USER_COMMON_OBJS = $(BUILD_LOGISIM)/user/user_crt0.user.o $(USER_LIB_OBJS)

$(BUILD_LOGISIM)/user/%.user.o: user/%.c user/libc.h | $(BUILD_LOGISIM)/user/
	$(RISCV_TOOL_PREFIX)gcc $(USER_CFLAGS) -Iuser -c $< -o $@

$(BUILD_LOGISIM)/user/%.user.o: user/%.S kernel/logisim/syscall_nr.h | $(BUILD_LOGISIM)/user/
	$(RISCV_TOOL_PREFIX)gcc $(USER_CFLAGS) -Iuser -Ikernel -Ikernel/logisim -c $< -o $@

# Template for building a user program: $(call user-program,name)
# Expects user/<name>.user.o as program-specific object file.
# Produces build/<name>.elf (stripped) and build/<name>-debug.elf with .asm/.sym.
define user-program
$(1)_OBJS = $$(USER_COMMON_OBJS) $$(BUILD_LOGISIM)/user/$(1).user.o

$$(BUILD_LOGISIM)/$(1).elf: $$($(1)_OBJS) | $$(BUILD_LOGISIM)/
	$$(RISCV_TOOL_PREFIX)ld -nostdlib -o $$(BUILD_LOGISIM)/$(1)-debug.elf -T $$(USER_LINKER_SCRIPT) $$($(1)_OBJS)
	$$(RISCV_TOOL_PREFIX)objdump -S $$(BUILD_LOGISIM)/$(1)-debug.elf > $$(BUILD_LOGISIM)/$(1).asm
	$$(RISCV_TOOL_PREFIX)nm $$(BUILD_LOGISIM)/$(1)-debug.elf > $$(BUILD_LOGISIM)/$(1).sym
	$$(RISCV_TOOL_PREFIX)strip -s $$(BUILD_LOGISIM)/$(1)-debug.elf -o $$(BUILD_LOGISIM)/$(1).elf
endef

#USER_PROGRAMS = hello spawn_demo shell ls mkdir rmdir mknod env_demo print rm dis_test

USER_PROGRAMS = hello shell

$(foreach prog,$(USER_PROGRAMS),$(eval $(call user-program,$(prog))))

USER_PROGRAM_ELFS = $(addprefix $(BUILD_LOGISIM)/,$(addsuffix .elf,$(USER_PROGRAMS)))
ALL_USER_OBJS = $(foreach prog,$(USER_PROGRAMS),$($(prog)_OBJS))

user-programs: $(USER_PROGRAM_ELFS)

check-pie: $(ALL_USER_OBJS)
	@echo "=== Checking user program object files for PIE compatibility ==="
	@FAILED=0; \
	for obj in $(ALL_USER_OBJS); do \
		if ! ./check_pie.sh $$obj > /dev/null 2>&1; then \
			./check_pie.sh $$obj; \
			FAILED=1; \
		fi; \
	done; \
	if [ $$FAILED -eq 0 ]; then \
		echo "All user program object files are PIE-compatible."; \
	else \
#		echo "ERROR: not PIE-compatible";\
		exit 1; \
	fi

# ====================================
# 4. Host tools (native filesystem utility)
# ====================================

FSTOOL_NATIVE_SRCS = fstool/fstool.c fstool/native_block.c
FSTOOL_KERNEL_SRCS = kernel/inode.c kernel/dir.c kernel/file.c kernel/fs.c kernel/device.c

FSTOOL_OBJS = $(patsubst %.c,$(BUILD_LOGISIM)/%.native.o,$(FSTOOL_NATIVE_SRCS)) \
              $(patsubst %.c,$(BUILD_LOGISIM)/%.native.o,$(FSTOOL_KERNEL_SRCS))

FSTOOL_HEADERS = kernel/fs.h kernel/fs_types.h kernel/logisim/block.h kernel/inode.h \
                 kernel/dir.h kernel/file.h fstool/native_block.h

$(BUILD_LOGISIM)/fstool/%.native.o: fstool/%.c $(FSTOOL_HEADERS) | $(BUILD_LOGISIM)/fstool/
	$(NATIVE_CC) -DNATIVE_BUILD -Wall -Wextra -O2 -iquote kernel -iquote fstool -iquote kernel/logisim -c $< -o $@

$(BUILD_LOGISIM)/kernel/%.native.o: kernel/%.c $(FSTOOL_HEADERS) | $(BUILD_LOGISIM)/kernel/
	$(NATIVE_CC) -DNATIVE_BUILD -Wall -Wextra -O2 -iquote kernel -iquote fstool -iquote kernel/logisim -c $< -o $@

FSTOOL_BIN = $(BUILD_LOGISIM)/fstool/fstool

$(FSTOOL_BIN): $(FSTOOL_OBJS) | $(BUILD_LOGISIM)/fstool/
	$(NATIVE_CC) -Wall -Wextra -O2 -o $@ $(FSTOOL_OBJS)
	@echo in fstool creation

# ====================================
# 5. Filesystem image (stays in project root)
# ====================================

fs-image: $(FSTOOL_BIN) $(BUILD_LOGISIM)/kernel.bin $(USER_PROGRAM_ELFS)
	$(FSTOOL_BIN) format block_storage.bin 256
	$(FSTOOL_BIN) mkdir block_storage.bin /boot
	$(FSTOOL_BIN) mkdir block_storage.bin /bin
	$(FSTOOL_BIN) add block_storage.bin /boot/kernel $(BUILD_LOGISIM)/kernel.bin
	$(FSTOOL_BIN) add block_storage.bin /bin/hello $(BUILD_LOGISIM)/hello.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/spawn_demo $(BUILD_LOGISIM)/spawn_demo.elf
	$(FSTOOL_BIN) add block_storage.bin /bin/sh $(BUILD_LOGISIM)/shell.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/ls $(BUILD_LOGISIM)/ls.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/mkdir $(BUILD_LOGISIM)/mkdir.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/rmdir $(BUILD_LOGISIM)/rmdir.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/mknod $(BUILD_LOGISIM)/mknod.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/env_demo $(BUILD_LOGISIM)/env_demo.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/print $(BUILD_LOGISIM)/print.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/rm $(BUILD_LOGISIM)/rm.elf
#	$(FSTOOL_BIN) add block_storage.bin /bin/dis_test $(BUILD_LOGISIM)/dis_test.elf
	@echo "Filesystem image created."

# ====================================
# Build everything
# ====================================

all: $(BUILD_LOGISIM)/boot-rom.txt fs-image

# ====================================
# Clean targets
# ====================================

clean:
	rm -rf $(BUILD_LOGISIM)

clean-all: clean
	rm -f block_storage.bin
