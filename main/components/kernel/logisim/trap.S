/*
 * Assembly language trap handler for RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Handles interrupts, exceptions, and system calls (ecall).
 * Adapted from multitasking example.
 */

    .text

/*
 * void set_trap_handler(void (*handler)(void), trap_frame_t *tf)
 *
 * Stores the address of assembly trap handler in mtvec register
 * and the address of trap_frame structure in mscratch register.
 *
 * a0 = address of trap_handler function
 * a1 = address of trap_frame structure
 */
    .globl set_trap_handler
set_trap_handler:
    csrw mtvec, a0
    csrw mscratch, a1
    ret

/*
 * void set_mie(uint32_t mie_value)
 * uint32_t get_mie(void)
 *
 * Read/write machine interrupt enable register.
 */
    .globl set_mie
set_mie:
    csrw mie, a0
    ret

    .globl get_mie
get_mie:
    csrr a0, mie
    ret

/*
 * void set_mstatus_bit(uint32_t mask)
 *
 * Set bits in mstatus register.
 */
    .globl set_mstatus_bit
set_mstatus_bit:
    csrs mstatus, a0
    ret

/*
 * void enable_interrupts(void)
 * void disable_interrupts(void)
 *
 * Globally enable/disable interrupt handling via mstatus.MIE bit.
 */
    .globl enable_interrupts
enable_interrupts:
    csrsi mstatus, 8    /* Set MIE bit (bit 3) */
    ret

    .globl disable_interrupts
disable_interrupts:
    csrci mstatus, 8    /* Clear MIE bit (bit 3) */
    ret

/*
 * uint32_t get_mcause(void)
 *
 * Return value of mcause register (reason for trap).
 */
    .globl get_mcause
get_mcause:
    csrr a0, mcause
    ret

/*
 * trap_handler - Entry point for all interrupts, exceptions, and ecalls.
 *
 * Saves complete CPU context to trap_frame, switches to kernel stack,
 * then calls C trap handler c_trap_handler(trap_frame_t *tf).
 */
    .globl trap_handler
trap_handler:
    /* Atomically swap a0 and mscratch.
     * After this: a0 = pointer to trap_frame, mscratch = old a0 value */
    csrrw a0, mscratch, a0

    /* Save all general-purpose registers to trap_frame */
    sw ra,  20(a0)
    sw sp,  24(a0)
    sw gp,  28(a0)
    sw tp,  32(a0)
    sw t0,  36(a0)
    sw t1,  40(a0)
    sw t2,  44(a0)
    sw s0,  48(a0)
    sw s1,  52(a0)
    /* a0 saved separately below */
    sw a1,  60(a0)
    sw a2,  64(a0)
    sw a3,  68(a0)
    sw a4,  72(a0)
    sw a5,  76(a0)
    sw a6,  80(a0)
    sw a7,  84(a0)
    sw s2,  88(a0)
    sw s3,  92(a0)
    sw s4,  96(a0)
    sw s5, 100(a0)
    sw s6, 104(a0)
    sw s7, 108(a0)
    sw s8, 112(a0)
    sw s9, 116(a0)
    sw s10, 120(a0)
    sw s11, 124(a0)
    sw t3, 128(a0)
    sw t4, 132(a0)
    sw t5, 136(a0)
    sw t6, 140(a0)

    /* Save mepc (exception program counter) to trap_frame */
    csrr t0, mepc
    sw t0, 12(a0)

    /* Save the original a0 value (now in mscratch) to trap_frame */
    csrr t0, mscratch
    sw t0, 56(a0)

    /* Switch to kernel stack (stored in trap_frame->c_trap_sp) */
    lw sp, 4(a0)

    /* Load address of C trap handler and call it with trap_frame pointer.
     * Use jalr so c_trap_handler can return here (for sys_exit). */
    lw t0, 8(a0)
    jalr ra, t0, 0

    /* c_trap_handler returned - this happens when sys_exit is called.
     * Load the saved kernel context address and jump to restore_kernel_context */
    la t0, kernel_context
    lw t0, 0(t0)
    j restore_kernel_context

/*
 * void trap_ret(trap_frame_t *tf)
 *
 * Restore CPU context from trap_frame and return to user program.
 * This function never returns - it jumps back to the interrupted code.
 *
 * a0 = pointer to trap_frame
 */
    .globl trap_ret
trap_ret:
    /* Restore mepc from trap_frame */
    lw t0, 12(a0)
    csrw mepc, t0

    /* Put saved a0 value in mscratch for final atomic swap */
    lw t0, 56(a0)
    csrw mscratch, t0

    /* Restore all general-purpose registers except a0 */
    lw ra,  20(a0)
    lw sp,  24(a0)
    lw gp,  28(a0)
    lw tp,  32(a0)
    lw t0,  36(a0)
    lw t1,  40(a0)
    lw t2,  44(a0)
    lw s0,  48(a0)
    lw s1,  52(a0)
    /* a0 restored via mscratch swap below */
    lw a1,  60(a0)
    lw a2,  64(a0)
    lw a3,  68(a0)
    lw a4,  72(a0)
    lw a5,  76(a0)
    lw a6,  80(a0)
    lw a7,  84(a0)
    lw s2,  88(a0)
    lw s3,  92(a0)
    lw s4,  96(a0)
    lw s5, 100(a0)
    lw s6, 104(a0)
    lw s7, 108(a0)
    lw s8, 112(a0)
    lw s9, 116(a0)
    lw s10, 120(a0)
    lw s11, 124(a0)
    lw t3, 128(a0)
    lw t4, 132(a0)
    lw t5, 136(a0)
    lw t6, 140(a0)

    /* Atomically restore a0 and save trap_frame pointer back to mscratch */
    csrrw a0, mscratch, a0

    /* Return from machine-mode trap to the address in mepc */
    mret

/*
 * int run_user_program(trap_frame_t *tf)
 *
 * Saves kernel context, starts user program via trap_ret.
 * When sys_exit is called, control returns here via restore_kernel_context.
 *
 * a0 = pointer to trap_frame (set up for user program)
 *
 * Returns: 0 (actual exit code retrieved via syscall_get_exit_code)
 */
    .globl run_user_program
run_user_program:
    /* Save all callee-saved registers on kernel stack */
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

    /* Save stack pointer to kernel_context so we can restore it on sys_exit */
    la   t0, kernel_context
    sw   sp, 0(t0)

    /* Jump to user program via trap_ret (never returns normally) */
    j    trap_ret

/*
 * restore_kernel_context - jumped to from trap_handler when c_trap_handler returns
 *
 * t0 = saved kernel stack pointer
 */
restore_kernel_context:
    mv   sp, t0

    /* Restore callee-saved registers */
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

    /* Return 0 - actual exit code is retrieved via syscall_get_exit_code */
    li   a0, 0
    ret

    .size run_user_program, .-run_user_program

    /* Storage for kernel stack pointer */
    .section .bss
    .align 4
kernel_context:
    .word 0
