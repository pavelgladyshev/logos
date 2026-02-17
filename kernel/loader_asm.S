# ELF Loader execution trampoline
# Licensed under Creative Commons Attribution International License 4.0
#
# Switches context to execute a loaded program and returns when it finishes.

    .text
    .globl elf_trampoline
    .type elf_trampoline, @function

# int elf_trampoline(uint32_t entry, uint32_t sp)
# a0 = entry point address of loaded program
# a1 = stack pointer for loaded program
#
# Returns: value returned by loaded program (in a0)

elf_trampoline:
    # Save all callee-saved registers and return address on loader's stack
    # s0-s11 (12 registers) + ra = 13 registers * 4 bytes = 52 bytes, round to 64
    addi sp, sp, -64
    sw   ra,  60(sp)
    sw   s0,  56(sp)
    sw   s1,  52(sp)
    sw   s2,  48(sp)
    sw   s3,  44(sp)
    sw   s4,  40(sp)
    sw   s5,  36(sp)
    sw   s6,  32(sp)
    sw   s7,  28(sp)
    sw   s8,  24(sp)
    sw   s9,  20(sp)
    sw   s10, 16(sp)
    sw   s11, 12(sp)

    # Save entry point in s0 (we'll restore it later anyway)
    mv   s0, a0

    # Save loader's stack pointer in a global variable
    # (can't rely on s-registers because user program has different stack)
    la   t0, loader_sp_save
    sw   sp, 0(t0)

    # Switch to program's stack
    mv   sp, a1

    # Clear argument registers (program gets no arguments)
    li   a0, 0
    li   a1, 0
    li   a2, 0

    # Jump to loaded program's entry point
    # jalr saves return address in ra, so program can return with 'ret'
    jalr ra, s0, 0

    # Program returned - a0 contains return value
    # Save return value in t1 (caller-saved, but we're done calling)
    mv   t1, a0

    # Restore loader's stack from global variable
    la   t0, loader_sp_save
    lw   sp, 0(t0)

    # Put return value back in a0
    mv   a0, t1

    # Restore all callee-saved registers
    lw   s11, 12(sp)
    lw   s10, 16(sp)
    lw   s9,  20(sp)
    lw   s8,  24(sp)
    lw   s7,  28(sp)
    lw   s6,  32(sp)
    lw   s5,  36(sp)
    lw   s4,  40(sp)
    lw   s3,  44(sp)
    lw   s2,  48(sp)
    lw   s1,  52(sp)
    lw   s0,  56(sp)
    lw   ra,  60(sp)
    addi sp, sp, 64

    ret

    .size elf_trampoline, .-elf_trampoline

    # Storage for loader's stack pointer during user program execution
    .section .bss
    .align 4
loader_sp_save:
    .word 0
