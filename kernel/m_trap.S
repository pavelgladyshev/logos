/*
 * M-mode trap handler and CSR wrappers for SV32 virtual memory
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * This file contains:
 * 1. A minimal M-mode trap handler that forwards timer interrupts
 *    to S-mode by setting SSIP (supervisor software interrupt pending).
 * 2. CSR accessor functions for S-mode registers (satp, stvec, etc.)
 * 3. Privilege mode transition helpers.
 */

    .text

/* ============================
 * M-mode timer forwarding handler
 * ============================
 *
 * Installed in mtvec permanently. When a machine timer interrupt fires:
 *   1. Set mtimecmp to mtime + TIME_SLICE to schedule the next tick
 *      (both have 64 bits, and are read and written a word at a time)
 *   2. Set MIP.SSIP (bit 1): S-mode takes a supervisor software interrupt
 *   3. mret back to S-mode / U-mode
 *
 * All other traps reaching M-mode are unexpected (everything should be
 * delegated) — halt with an error.
 */

#include "timer.h"

#define TIMER_MTIME     TIMER_MTIME_ADDR
#define TIMER_MTIMECMP  TIMER_MTIMECMP_ADDR

    .globl m_trap_handler
    .align 4
m_trap_handler:
    /* Save t0, t1 and t2 — we use all three in the timer handler.
     * t0 is saved in mscratch, t1 and t2 in m_trap_save.
     *
     * They must NOT be pushed on the interrupted stack: if the timer fires
     * while a user program runs, sp holds a user *virtual* address
     * (around 0x40FFxx), but M-mode accesses are not translated, so the
     * push and pop would go to that *physical* address, where there is no
     * RAM, and they would come back as garbage.  The kernel is linked at its
     * physical address, so static save slots work in M-mode.  Only one
     * hart, and M-mode interrupts are off inside this handler, so one slot
     * each is enough. */
    csrw mscratch, t0
    la   t0, m_trap_save
    sw   t1, 0(t0)
    sw   t2, 4(t0)

    /* Check if this is a timer interrupt (mcause = 0x80000007) */
    csrr t0, mcause
    li   t1, 0x80000007       /* Machine timer interrupt */
    bne  t0, t1, m_trap_unexpected

    /* Timer interrupt: set mtimecmp to mtime + TIME_SLICE. Both registers
     * have 64 bits, in two words each: the low word at the register's
     * address and the high word 4 above it (timer.h says more).
     *
     * mtimecmp is set with the three stores that the RISC-V privileged
     * specification gives for a 32-bit RISC-V. This is the first: all ones
     * into the low word, so that no value on the way is below both the old
     * value and the new one, and the timer requests no interrupt in between. */
    li   t0, TIMER_MTIMECMP
    li   t1, -1
    sw   t1, 0(t0)            /* mtimecmp, low word: all ones for now */

    /* Read mtime. It counts on while its two words are read, and the low
     * word may carry into the high word in between: read the high word, the
     * low word and the high word again, until the high word has not changed. */
1:  li   t0, TIMER_MTIME
    lw   t1, 4(t0)            /* t1 = mtime, high word */
    lw   t2, 0(t0)            /* t2 = mtime, low word */
    lw   t0, 4(t0)            /* the high word again */
    bne  t0, t1, 1b           /* it changed: read again */

    /* Add TIME_SLICE, with the carry from the low word into the high word */
    li   t0, TIME_SLICE
    add  t2, t2, t0           /* low word + TIME_SLICE */
    sltu t0, t2, t0           /* t0 = 1 if the sum is below TIME_SLICE: a carry */
    add  t1, t1, t0           /* t1:t2 = mtime + TIME_SLICE */

    /* The second and the third store: the high word, then the low word */
    li   t0, TIMER_MTIMECMP
    sw   t1, 4(t0)            /* mtimecmp, high word */
    sw   t2, 0(t0)            /* mtimecmp, low word: the new value */

    /* Inject supervisor software interrupt to forward timer tick to S-mode.
     * We use SSIP (bit 1) instead of STIP (bit 5) because STIP is read-only
     * from S-mode — the S-mode handler couldn't clear it, causing an infinite
     * trap loop after sret re-enables interrupts. SSIP is writable from S-mode. */
    li   t0, (1 << 1)         /* SSIP bit */
    csrs mip, t0              /* Set SSIP — S-mode will see this as software interrupt */

    /* Restore t2, t1 and t0, then return */
    la   t0, m_trap_save
    lw   t2, 4(t0)
    lw   t1, 0(t0)
    csrr t0, mscratch
    mret

m_trap_unexpected:
    /* Unexpected trap in M-mode — should not happen if delegation is correct */
    /* Halt (infinite loop) */
    la   t0, m_trap_save
    lw   t2, 4(t0)
    lw   t1, 0(t0)
    csrr t0, mscratch         /* restore t0 before halting */
1:  j    1b

/* ============================
 * S-mode CSR accessors
 * ============================ */

    .globl write_satp
write_satp:
    csrw 0x180, a0     /* satp = a0 */
    ret

    .globl sfence_vma
sfence_vma:
    .word 0x12000073   /* sfence.vma zero, zero */
    ret

    .globl read_stval
read_stval:
    csrr a0, 0x143     /* a0 = stval */
    ret

/* ============================
 * S-mode interrupt enable
 * ============================ */

/* void write_sie(uint32_t val) — set supervisor interrupt enable register */
    .globl write_sie
write_sie:
    csrw 0x104, a0     /* sie = a0 */
    ret

/* ============================
 * M-mode delegation setup
 * ============================ */

/*
 * void set_mtvec(uint32_t handler)
 *
 * Write mtvec CSR (M-mode trap vector). Used to install
 * the M-mode timer forwarder.
 */
    .globl set_mtvec
set_mtvec:
    csrw mtvec, a0
    ret

    .globl write_medeleg
write_medeleg:
    csrw 0x302, a0     /* medeleg = a0 */
    ret

    .globl write_mideleg
write_mideleg:
    csrw 0x303, a0     /* mideleg = a0 */
    ret

/* ============================
 * Privilege mode transitions
 * ============================ */

/*
 * void clear_spp(void)
 *
 * Clear sstatus.SPP to 0 (User mode).
 * After sret, CPU enters U-mode.
 */
    .globl clear_spp
clear_spp:
    li   t0, (1 << 8)         /* SPP bit */
    csrc 0x100, t0             /* sstatus &= ~SPP */
    ret

/*
 * void set_spie(void)
 *
 * Set sstatus.SPIE to 1 so sret enables interrupts.
 */
    .globl set_spie
set_spie:
    li   t0, (1 << 5)         /* SPIE bit */
    csrs 0x100, t0             /* sstatus |= SPIE */
    ret

/*
 * void clear_ssip(void)
 *
 * Clear SIP.SSIP (bit 1) to acknowledge the supervisor software interrupt.
 * SSIP is writable from S-mode (unlike STIP which is read-only from S-mode).
 */
    .globl clear_ssip
clear_ssip:
    li   t0, (1 << 1)         /* SSIP bit */
    csrc 0x144, t0             /* sip &= ~SSIP */
    ret

/*
 * void clear_stip(void)
 *
 * Clear SIP.STIP (bit 5). Note: STIP may be read-only from S-mode
 * depending on implementation. Prefer clear_ssip() for timer forwarding.
 */
    .globl clear_stip
clear_stip:
    li   t0, (1 << 5)         /* STIP bit */
    csrc 0x144, t0             /* sip &= ~STIP */
    ret

/*
 * void drop_to_smode(uint32_t entry)
 *
 * Set mepc to entry address and execute mret to transition from
 * M-mode to S-mode. Caller must have set mstatus.MPP=Supervisor
 * and mstatus.MPIE=0 beforehand.
 *
 * Never returns.
 */
    .globl drop_to_smode
drop_to_smode:
    csrw mepc, a0
    mret

/*
 * Note on timer forwarding:
 * The M-mode timer handler forwards timer ticks to S-mode using SSIP
 * (supervisor software interrupt), NOT STIP. This is because STIP is
 * read-only from S-mode — the S-mode handler cannot clear it, which
 * would cause an infinite trap loop after sret re-enables interrupts.
 * SSIP is writable from S-mode, so the S-mode handler can clear it
 * via clear_ssip() before returning.
 */

/* M-mode save slots for t1 and t2 (see m_trap_handler). */
    .data
    .align 2
m_trap_save:
    .word 0
    .word 0
