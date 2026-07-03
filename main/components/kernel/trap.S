.section .exception_vectors_table.text, "ax"
.balign 0x100
.option push
.option norvc

.global kernel_vector_table
.type kernel_vector_table, @function

/* mtvec points here. Every vector currently funnels into the common entry. */
kernel_vector_table:
  j trap_entry        /* 0: synchronous exception, including ecall */
  j trap_entry        /* 1 */
  j trap_entry        /* 2 */
  j trap_entry        /* 3 */
  j trap_entry        /* 4 */
  j trap_entry        /* 5 */
  j trap_entry        /* 6 */
  j trap_entry        /* 7 */
  j trap_entry        /* 8 */
  j trap_entry        /* 9 */
  j trap_entry        /* 10 */
  j trap_entry        /* 11 */
  j trap_entry        /* 12 */
  j trap_entry        /* 13 */
  j trap_entry        /* 14 */
  j trap_entry        /* 15 */
  j trap_entry        /* 16 */
  j trap_entry        /* 17 */
  j trap_entry        /* 18 */
  j trap_entry        /* 19 */
  j trap_entry        /* 20 */
  j trap_entry        /* 21 */
  j trap_entry        /* 22 */
  j trap_entry        /* 23 */
  j trap_entry        /* 24 */
  j trap_entry        /* 25 */
  j trap_entry        /* 26 */
  j trap_entry        /* 27 */
  j trap_entry        /* 28 */
  j trap_entry        /* 29 */
  j trap_entry        /* 30 */
  j trap_entry        /* 31 */


    .size kernel_vector_table, . - kernel_vector_table
  .option pop
  .section .exception_vectors.text, "ax"

.global trap_entry
.type trap_entry, @function

.equ TF_RA_OFFSET,       0
.equ TF_SP_OFFSET,       4
.equ TF_GP_OFFSET,       8
.equ TF_TP_OFFSET,       12
.equ TF_T0_OFFSET,       16
.equ TF_T1_OFFSET,       20
.equ TF_T2_OFFSET,       24
.equ TF_S0_OFFSET,       28
.equ TF_S1_OFFSET,       32
.equ TF_A0_OFFSET,       36
.equ TF_A1_OFFSET,       40
.equ TF_A2_OFFSET,       44
.equ TF_A3_OFFSET,       48
.equ TF_A4_OFFSET,       52
.equ TF_A5_OFFSET,       56
.equ TF_A6_OFFSET,       60
.equ TF_A7_OFFSET,       64
.equ TF_S2_OFFSET,       68
.equ TF_S3_OFFSET,       72
.equ TF_S4_OFFSET,       76
.equ TF_S5_OFFSET,       80
.equ TF_S6_OFFSET,       84
.equ TF_S7_OFFSET,       88
.equ TF_S8_OFFSET,       92
.equ TF_S9_OFFSET,       96
.equ TF_S10_OFFSET,      100
.equ TF_S11_OFFSET,      104
.equ TF_T3_OFFSET,       108
.equ TF_T4_OFFSET,       112
.equ TF_T5_OFFSET,       116
.equ TF_T6_OFFSET,       120
.equ TF_MEPC_OFFSET,     124
.equ TF_MSTATUS_OFFSET,  128
.equ TF_MCAUSE_OFFSET,   132
.equ TF_MTVAL_OFFSET,    136
.equ TF_SIZE,            144
trap_entry:
    /*
     * Swap from the interrupted stack to the kernel trap stack.
     * trap_install() keeps mscratch loaded with the trap-stack top.
     */
    csrrw sp, mscratch, sp 
    addi sp, sp, -TF_SIZE

    /* Save the interrupted register state into trap_frame_t. */
    sw ra, TF_RA_OFFSET(sp)

    sw t0, TF_T0_OFFSET(sp)
    /* After csrrw, mscratch contains the interrupted stack pointer. */
    csrr t0, mscratch

    sw t0, TF_SP_OFFSET(sp)
    sw gp, TF_GP_OFFSET(sp)
    sw tp, TF_TP_OFFSET(sp)
    sw t1, TF_T1_OFFSET(sp)
    sw t2, TF_T2_OFFSET(sp)
    sw s0, TF_S0_OFFSET(sp)
    sw s1, TF_S1_OFFSET(sp)
    sw a0, TF_A0_OFFSET(sp)
    sw a1, TF_A1_OFFSET(sp)
    sw a2, TF_A2_OFFSET(sp)
    sw a3, TF_A3_OFFSET(sp)
    sw a4, TF_A4_OFFSET(sp)
    sw a5, TF_A5_OFFSET(sp)
    sw a6, TF_A6_OFFSET(sp)
    sw a7, TF_A7_OFFSET(sp)
    sw s2, TF_S2_OFFSET(sp)
    sw s3, TF_S3_OFFSET(sp)
    sw s4, TF_S4_OFFSET(sp)
    sw s5, TF_S5_OFFSET(sp)
    sw s6, TF_S6_OFFSET(sp)
    sw s7, TF_S7_OFFSET(sp)
    sw s8, TF_S8_OFFSET(sp)
    sw s9, TF_S9_OFFSET(sp)
    sw s10, TF_S10_OFFSET(sp)
    sw s11, TF_S11_OFFSET(sp)
    sw t3, TF_T3_OFFSET(sp)
    sw t4, TF_T4_OFFSET(sp)
    sw t5, TF_T5_OFFSET(sp)
    sw t6, TF_T6_OFFSET(sp)
    
    csrr t0, mepc
    sw t0, TF_MEPC_OFFSET(sp)

    csrr t0, mstatus
    sw t0, TF_MSTATUS_OFFSET(sp)

    csrr t0, mcause
    sw t0, TF_MCAUSE_OFFSET(sp)

    csrr t0, mtval
    sw t0, TF_MTVAL_OFFSET(sp)
    
    /* C receives a pointer to trap_frame_t and may edit mepc/a0/etc. */
    mv a0, sp
    call c_trap_handler

    /* Restore machine CSRs from the possibly edited trap frame. */
    lw t0, TF_MEPC_OFFSET(sp)
    csrw mepc, t0

    lw t0, TF_MSTATUS_OFFSET(sp)
    csrw mstatus, t0

    /*
     * Re-arm mscratch with the trap-stack top before restoring t0.
     * The next trap then has a valid kernel stack again.
     */
    addi t0, sp, TF_SIZE
    csrw mscratch, t0

    /* Restore general registers and return to the interrupted context. */
    lw ra, TF_RA_OFFSET(sp)
    lw gp, TF_GP_OFFSET(sp)
    lw tp, TF_TP_OFFSET(sp)
    lw t0, TF_T0_OFFSET(sp)
    lw t1, TF_T1_OFFSET(sp)
    lw t2, TF_T2_OFFSET(sp)
    lw s0, TF_S0_OFFSET(sp)
    lw s1, TF_S1_OFFSET(sp)
    lw a0, TF_A0_OFFSET(sp)
    lw a1, TF_A1_OFFSET(sp)
    lw a2, TF_A2_OFFSET(sp)
    lw a3, TF_A3_OFFSET(sp)
    lw a4, TF_A4_OFFSET(sp)
    lw a5, TF_A5_OFFSET(sp)
    lw a6, TF_A6_OFFSET(sp)
    lw a7, TF_A7_OFFSET(sp)
    lw s2, TF_S2_OFFSET(sp)
    lw s3, TF_S3_OFFSET(sp)
    lw s4, TF_S4_OFFSET(sp)
    lw s5, TF_S5_OFFSET(sp)
    lw s6, TF_S6_OFFSET(sp)
    lw s7, TF_S7_OFFSET(sp)
    lw s8, TF_S8_OFFSET(sp)
    lw s9, TF_S9_OFFSET(sp)
    lw s10, TF_S10_OFFSET(sp)
    lw s11, TF_S11_OFFSET(sp)
    lw t3, TF_T3_OFFSET(sp)
    lw t4, TF_T4_OFFSET(sp)
    lw t5, TF_T5_OFFSET(sp)
    lw t6, TF_T6_OFFSET(sp)
    lw sp, TF_SP_OFFSET(sp)
    mret
