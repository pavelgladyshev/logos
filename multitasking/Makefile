# Makefile for software running in Logisim RISC-V Computer System model by Pavel Gladyshev
# licensed under Creative Commons Attribution International license 4.0
#
# This Makefile is designed to be used in Ubuntu Linux. Make sure that all required 
# software packages are installed before running this file. To install necessary packages 
# in Ubuntu Linux, simply run the following command:
#
# sudo apt install build-essential binutils-riscv64-unknown-elf gcc-riscv64-unknown-elf
#
# To build the application, cd into the folder with this file and run make
#
# If you want to change the name of the executable or add/remove source files in this project,
# edit the Application-specific detail section below. 

# --- User program 1 ---

USER1_ELF = user1
USER1_BIN = $(USER1_ELF).bin
USER1_OBJECT_FILES = $(USER1_ELF).o crt0.o
USER1_LINKER_SCRIPT = user1.ld

$(USER1_ELF) : $(USER1_OBJECT_FILES) 
	${RISCV_TOOL_PREFIX}ld -nostdlib -o $(USER1_ELF) -Map $(USER1_ELF).map -T $(USER1_LINKER_SCRIPT) $^
	${RISCV_TOOL_PREFIX}objdump -S $(USER1_ELF) > $(USER1_ELF).asm

# rule and command to extract all code and constant data sections from the ELF executable and place them into a plain binary file
$(USER1_BIN) : $(USER1_ELF)
	${RISCV_TOOL_PREFIX}objcopy  --output-target binary $(USER1_ELF) $(USER1_BIN)

# --- User program 2 ---

USER2_ELF = user2
USER2_BIN = $(USER2_ELF).bin
USER2_OBJECT_FILES = $(USER2_ELF).o crt0.o
USER2_LINKER_SCRIPT = user2.ld

$(USER2_ELF) : $(USER2_OBJECT_FILES) 
	${RISCV_TOOL_PREFIX}ld -nostdlib -o $(USER2_ELF) -Map $(USER2_ELF).map -T $(USER2_LINKER_SCRIPT) $^
	${RISCV_TOOL_PREFIX}objdump -S $(USER2_ELF) > $(USER2_ELF).asm

# rule and command to extract all code and constant data sections from the ELF executable and place them into a plain binary file
$(USER2_BIN) : $(USER2_ELF)
	${RISCV_TOOL_PREFIX}objcopy  --output-target binary $(USER2_ELF) $(USER2_BIN)


# --- User program 3 ---

USER3_ELF = user3
USER3_BIN = $(USER3_ELF).bin
USER3_OBJECT_FILES = $(USER3_ELF).o crt0.o
USER3_LINKER_SCRIPT = user3.ld

$(USER3_ELF) : $(USER3_OBJECT_FILES) 
	${RISCV_TOOL_PREFIX}ld -nostdlib -o $(USER3_ELF) -Map $(USER3_ELF).map -T $(USER3_LINKER_SCRIPT) $^
	${RISCV_TOOL_PREFIX}objdump -S $(USER3_ELF) > $(USER3_ELF).asm

# rule and command to extract all code and constant data sections from the ELF executable and place them into a plain binary file 
$(USER3_BIN) : $(USER3_ELF)
	${RISCV_TOOL_PREFIX}objcopy  --output-target binary $(USER3_ELF) $(USER3_BIN)


# --- Supervisor ----

# name of the executable file
SUPERVISOR_ELF = supervisor

# name of the binary file to hold .text and other executable sections of the supervisor that will be placed in ROM
SUPERVISOR_BIN = $(SUPERVISOR_ELF).bin

# list of all *object* files that must be produced and linked together into the executable.
# for *each* object file in this list there must be a corresponding C (.c) or Assembly (.S or .s) source file
SUPERVISOR_OBJECT_FILES = crt0.o supervisor.o trap.o

# list of all header files (.h) and, possibly, other data files *included* into the source code files
# via .include or #include directives
SUPERVISOR_HEADER_FILES = vga.h console.h trap.h timer.h

# linker script for the supervisor program
SUPERVISOR_LINKER_SCRIPT = supervisor.ld

# rule that says that all object files of the supervisor need to be recompiled if any of its header files change.
$(SUPERVISOR_OBJECT_FILES) : $(SUPERVISOR_HEADER_FILES)

# rule and commands to generate executable file (ELF), disassemble it into .asm file, create linker MAP, and symbol table
$(SUPERVISOR_ELF) : $(SUPERVISOR_OBJECT_FILES) 
	${RISCV_TOOL_PREFIX}ld -nostdlib -o $(SUPERVISOR_ELF) -Map $(SUPERVISOR_ELF).map -T $(SUPERVISOR_LINKER_SCRIPT) $^
	${RISCV_TOOL_PREFIX}objdump -S $(SUPERVISOR_ELF) > $(SUPERVISOR_ELF).asm

# rule and command to extract all code and constant data sections from the ELF executable and place them into a plain binary file (ROM_IMAGE)
$(SUPERVISOR_BIN) : $(SUPERVISOR_ELF)
	${RISCV_TOOL_PREFIX}objcopy --only-section .init --only-section .text --only-section .rodata --only-section .srodata --only-section .data --only-section .sdata --only-section .text.* --only-section .rodata.* --only-section .srodata.* --only-section .data.* --only-section .sdata.*  --output-target binary $(SUPERVISOR_ELF) $(SUPERVISOR_BIN)

# -------------------------

all: $(SUPERVISOR_BIN) $(USER1_BIN) $(USER2_BIN) $(USER3_BIN)

# what will be built, if make command is run without parameters
.DEFAULT_GOAL = all

# Tool name prfix
RISCV_TOOL_PREFIX=riscv64-unknown-elf-

# RISC-V instruction set architecture to use when compiling the program (RV32IM by default)
RISCV_ISA=rv32imzicsr

# RISC-V function calling convention to use when compiling the program (ILP32 by default)
RISCV_ABI=ilp32

# Additional command line options for GCC
CFLAGS=

# rule and command to compile C files
%.o : %.c
	${RISCV_TOOL_PREFIX}gcc -O -march=$(RISCV_ISA) -mabi=$(RISCV_ABI) -g -c $(CFLAGS) $< -o $@

# rule and command to process .S assembly files using GNU C compiler (.S files may contain C-style #include and #define statements)
%.o : %.S
	${RISCV_TOOL_PREFIX}gcc -O -march=$(RISCV_ISA) -mabi=$(RISCV_ABI) -g -c $(CFLAGS) $< -o $@
	
# rule and command to process .s assembly files using GNU Assembler (.s files must use pure assembly language syntax)
%.o : %.s
	${RISCV_TOOL_PREFIX}as -march=$(RISCV_ISA) -mabi=$(RISCV_ABI) -g -c $< -o $@
	
# command to delete all executable and object files and their derivatives as a way of forcing complete rebuild of the project
clean: 
	rm -f $(SUPERVISOR_OBJECT_FILES) $(SUPERVISOR_ELF) $(SUPERVISOR_BIN) $(SUPERVISOR_ELF).map $(SUPERVISOR_ELF).asm \
$(USER1_OBJECT_FILES) $(USER1_ELF) $(USER1_BIN) $(USER1_ELF).map $(USER1_ELF).asm \
$(USER2_OBJECT_FILES) $(USER2_ELF) $(USER2_BIN) $(USER2_ELF).map $(USER2_ELF).asm \
$(USER3_OBJECT_FILES) $(USER3_ELF) $(USER3_BIN) $(USER3_ELF).map $(USER3_ELF).asm \
