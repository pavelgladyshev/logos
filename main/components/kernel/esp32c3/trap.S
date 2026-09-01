.section .exception_vectors.text, "ax"
.option push
.option norvc

.equ TF_RESERVED_OFFSET,  0
.equ TF_C_TRAP_SP_OFFSET, 4
.equ TF_C_TRAP_OFFSET,    8
.equ TF_MEPC_OFFSET,      12
.equ TF_MSTATUS_OFFSET,   16
.equ TF_RA_OFFSET,        20
.equ TF_SP_OFFSET,        24
.equ TF_GP_OFFSET,        28
.equ TF_TP_OFFSET,        32
.equ TF_T0_OFFSET,        36
.equ TF_T1_OFFSET,        40
.equ TF_T2_OFFSET,        44
.equ TF_S0_OFFSET,        48
.equ TF_S1_OFFSET,        52
.equ TF_A0_OFFSET,        56
.equ TF_A1_OFFSET,        60
.equ TF_A2_OFFSET,        64
.equ TF_A3_OFFSET,        68
.equ TF_A4_OFFSET,        72
.equ TF_A5_OFFSET,        76
.equ TF_A6_OFFSET,        80
.equ TF_A7_OFFSET,        84
.equ TF_S2_OFFSET,        88
.equ TF_S3_OFFSET,        92
.equ TF_S4_OFFSET,        96
.equ TF_S5_OFFSET,        100
.equ TF_S6_OFFSET,        104
.equ TF_S7_OFFSET,        108
.equ TF_S8_OFFSET,        112
.equ TF_S9_OFFSET,        116
.equ TF_S10_OFFSET,       120
.equ TF_S11_OFFSET,       124
.equ TF_T3_OFFSET,        128
.equ TF_T4_OFFSET,        132
.equ TF_T5_OFFSET,        136
.equ TF_T6_OFFSET,        140
.equ TF_MCAUSE_OFFSET,    144
.equ TF_MTVAL_OFFSET,     148

.equ MSTATUS_MPP_MASK,    0x1800
.equ MSTATUS_MPIE,        0x80

.global _logos_trap_start
_logos_trap_start:

/*
 * void set_trap_handler(void (*handler)(void), trap_frame_t *tf)
 *
 * mtvec receives the assembly trap entry. mscratch receives the active
 * trap_frame pointer. trap_ret keeps mscratch pointing at the current frame.
 */
.global set_trap_handler
.type set_trap_handler, @function
set_trap_handler:
    /* Direct mode sends every exception and interrupt to trap_handler. */
    csrw mtvec, a0
    csrw mscratch, a1
    ret

.global set_mie
.type set_mie, @function
set_mie:
    csrw mie, a0
    ret

.global get_mtvec
.type get_mtvec, @function
get_mtvec:
    csrr a0, mtvec
    ret

.global get_mie
.type get_mie, @function
get_mie:
    csrr a0, mie
    ret

.global set_mstatus_bit
.type set_mstatus_bit, @function
set_mstatus_bit:
    csrs mstatus, a0
    ret

.global enable_interrupts
.type enable_interrupts, @function
enable_interrupts:
    csrsi mstatus, 8
    ret

.global disable_interrupts
.type disable_interrupts, @function
disable_interrupts:
    csrci mstatus, 8
    ret

.global get_mcause
.type get_mcause, @function
get_mcause:
    csrr a0, mcause
    ret

.global kernel_vector_table
.type kernel_vector_table, @function
    .balign 0x100
kernel_vector_table:
    j trap_handler

.global trap_handler
.type trap_handler, @function
trap_handler:
    /*
     * a0 becomes the active trap_frame pointer. mscratch temporarily holds
     * the interrupted a0 value until it is saved below.
     */
    csrrw a0, mscratch, a0

    sw ra,  TF_RA_OFFSET(a0)
    sw sp,  TF_SP_OFFSET(a0)
    sw gp,  TF_GP_OFFSET(a0)
    sw tp,  TF_TP_OFFSET(a0)
    sw t0,  TF_T0_OFFSET(a0)
    sw t1,  TF_T1_OFFSET(a0)
    sw t2,  TF_T2_OFFSET(a0)
    sw s0,  TF_S0_OFFSET(a0)
    sw s1,  TF_S1_OFFSET(a0)
    sw a1,  TF_A1_OFFSET(a0)
    sw a2,  TF_A2_OFFSET(a0)
    sw a3,  TF_A3_OFFSET(a0)
    sw a4,  TF_A4_OFFSET(a0)
    sw a5,  TF_A5_OFFSET(a0)
    sw a6,  TF_A6_OFFSET(a0)
    sw a7,  TF_A7_OFFSET(a0)
    sw s2,  TF_S2_OFFSET(a0)
    sw s3,  TF_S3_OFFSET(a0)
    sw s4,  TF_S4_OFFSET(a0)
    sw s5,  TF_S5_OFFSET(a0)
    sw s6,  TF_S6_OFFSET(a0)
    sw s7,  TF_S7_OFFSET(a0)
    sw s8,  TF_S8_OFFSET(a0)
    sw s9,  TF_S9_OFFSET(a0)
    sw s10, TF_S10_OFFSET(a0)
    sw s11, TF_S11_OFFSET(a0)
    sw t3,  TF_T3_OFFSET(a0)
    sw t4,  TF_T4_OFFSET(a0)
    sw t5,  TF_T5_OFFSET(a0)
    sw t6,  TF_T6_OFFSET(a0)

    csrr t0, mepc
    sw t0, TF_MEPC_OFFSET(a0)
    csrr t0, mstatus
    sw t0, TF_MSTATUS_OFFSET(a0)
    csrr t0, mcause
    sw t0, TF_MCAUSE_OFFSET(a0)
    csrr t0, mtval
    sw t0, TF_MTVAL_OFFSET(a0)

    csrr t0, mscratch
    sw t0, TF_A0_OFFSET(a0)

    lw sp, TF_C_TRAP_SP_OFFSET(a0)

    /* Kernel C code must run with the kernel's global and thread pointers,
     * not the values restored for the interrupted process. */
    la t0, kernel_context
    lw gp, 4(t0)
    lw tp, 8(t0)

    lw t0, TF_C_TRAP_OFFSET(a0)
    jalr ra, t0, 0

    la t0, kernel_context
    lw t0, 0(t0)
    j restore_kernel_context

.global trap_ret
.type trap_ret, @function
trap_ret:
    /* a0 = trap_frame_t * */
    lw t0, TF_MEPC_OFFSET(a0)
    csrw mepc, t0

    lw t0, TF_MSTATUS_OFFSET(a0)
    ori t0, t0, MSTATUS_MPIE
    csrw mstatus, t0

    lw t0, TF_A0_OFFSET(a0)
    csrw mscratch, t0

    lw ra,  TF_RA_OFFSET(a0)
    lw sp,  TF_SP_OFFSET(a0)
    lw gp,  TF_GP_OFFSET(a0)
    lw tp,  TF_TP_OFFSET(a0)
    lw t0,  TF_T0_OFFSET(a0)
    lw t1,  TF_T1_OFFSET(a0)
    lw t2,  TF_T2_OFFSET(a0)
    lw s0,  TF_S0_OFFSET(a0)
    lw s1,  TF_S1_OFFSET(a0)
    lw a1,  TF_A1_OFFSET(a0)
    lw a2,  TF_A2_OFFSET(a0)
    lw a3,  TF_A3_OFFSET(a0)
    lw a4,  TF_A4_OFFSET(a0)
    lw a5,  TF_A5_OFFSET(a0)
    lw a6,  TF_A6_OFFSET(a0)
    lw a7,  TF_A7_OFFSET(a0)
    lw s2,  TF_S2_OFFSET(a0)
    lw s3,  TF_S3_OFFSET(a0)
    lw s4,  TF_S4_OFFSET(a0)
    lw s5,  TF_S5_OFFSET(a0)
    lw s6,  TF_S6_OFFSET(a0)
    lw s7,  TF_S7_OFFSET(a0)
    lw s8,  TF_S8_OFFSET(a0)
    lw s9,  TF_S9_OFFSET(a0)
    lw s10, TF_S10_OFFSET(a0)
    lw s11, TF_S11_OFFSET(a0)
    lw t3,  TF_T3_OFFSET(a0)
    lw t4,  TF_T4_OFFSET(a0)
    lw t5,  TF_T5_OFFSET(a0)
    lw t6,  TF_T6_OFFSET(a0)

    csrrw a0, mscratch, a0
    fence.i
    csrr t0, mepc
    jr t0

.global run_user_program
.type run_user_program, @function
run_user_program:
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

    la   t0, kernel_context
    sw   sp, 0(t0)
    sw   gp, 4(t0)
    sw   tp, 8(t0)
    j    trap_ret

restore_kernel_context:
    mv   sp, t0
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
    li   a0, 0
    ret

    .balign 4
.global _logos_trap_end
_logos_trap_end:

.option pop

.section .bss
.align 4
kernel_context:
    .word 0 /* kernel sp */
    .word 0 /* kernel gp */
    .word 0 /* kernel tp */

.section .note.GNU-stack,"",@progbits
