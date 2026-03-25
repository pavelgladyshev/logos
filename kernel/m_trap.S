/*
 * M-mode trap handler and CSR wrappers for SV32 virtual memory
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * This file contains:
 * 1. A minimal M-mode trap handler that forwards timer interrupts
 *    to S-mode by injecting STIP (supervisor timer interrupt pending).
 * 2. CSR accessor functions for S-mode registers (satp, stvec, etc.)
 * 3. Privilege mode transition helpers.
 */

    .text

/* ============================
 * M-mode timer forwarding handler
 * ============================
 *
 * Installed in mtvec permanently. When a machine timer interrupt fires:
 *   1. Advance mtimecmp to schedule the next tick
 *   2. Set MIP.STIP (bit 5) to inject supervisor timer interrupt
 *   3. mret back to S-mode / U-mode
 *
 * All other traps reaching M-mode are unexpected (everything should be
 * delegated) — halt with an error.
 */

#define TIMER_MTIME     0x200bff8
#define TIMER_MTIMECMP  0x2004000
#define TIME_SLICE      800

    .globl m_trap_handler
    .align 4
m_trap_handler:
    /* Save t0-t2 using mscratch as temporary storage */
    csrw mscratch, t0

    /* Check if this is a timer interrupt (mcause = 0x80000007) */
    csrr t0, mcause
    li   t1, 0x80000007       /* Machine timer interrupt */
    bne  t0, t1, m_trap_unexpected

    /* Timer interrupt: advance mtimecmp */
    li   t0, TIMER_MTIME
    lw   t1, 0(t0)            /* t1 = current mtime */
    li   t0, TIME_SLICE
    add  t1, t1, t0           /* t1 = mtime + TIME_SLICE */
    li   t0, TIMER_MTIMECMP
    sw   t1, 0(t0)            /* mtimecmp = mtime + TIME_SLICE */

    /* Clear any previous STIP, then set it to inject supervisor timer interrupt */
    li   t0, (1 << 5)         /* STIP bit */
    csrc mip, t0              /* Clear first (in case previous wasn't handled) */
    csrs mip, t0              /* Set STIP — S-mode will see this on sret */

    /* Restore t0 and return */
    csrr t0, mscratch
    mret

m_trap_unexpected:
    /* Unexpected trap in M-mode — should not happen if delegation is correct */
    /* Halt (infinite loop) */
    csrr t0, mscratch         /* restore t0 before halting */
    j    m_trap_unexpected

/* ============================
 * S-mode CSR accessors
 * ============================ */

    .globl write_satp
write_satp:
    csrw 0x180, a0     /* satp = a0 */
    ret

    .globl read_satp
read_satp:
    csrr a0, 0x180     /* a0 = satp */
    ret

    .globl sfence_vma
sfence_vma:
    .word 0x12000073   /* sfence.vma zero, zero */
    ret

    .globl write_stvec
write_stvec:
    csrw 0x105, a0     /* stvec = a0 */
    ret

    .globl write_sscratch
write_sscratch:
    csrw 0x140, a0     /* sscratch = a0 */
    ret

    .globl read_scause
read_scause:
    csrr a0, 0x142     /* a0 = scause */
    ret

    .globl read_stval
read_stval:
    csrr a0, 0x143     /* a0 = stval */
    ret

    .globl read_sepc
read_sepc:
    csrr a0, 0x141     /* a0 = sepc */
    ret

    .globl write_sepc
write_sepc:
    csrw 0x141, a0     /* sepc = a0 */
    ret

/* ============================
 * S-mode interrupt enable
 * ============================ */

/* void write_sie(uint32_t val) — set supervisor interrupt enable register */
    .globl write_sie
write_sie:
    csrw 0x104, a0     /* sie = a0 */
    ret

/* uint32_t read_sie(void) */
    .globl read_sie
read_sie:
    csrr a0, 0x104     /* a0 = sie */
    ret

/* void set_sstatus_bit(uint32_t mask) — set bits in sstatus */
    .globl set_sstatus_bit
set_sstatus_bit:
    csrs 0x100, a0     /* sstatus |= a0 */
    ret

/* void clear_sstatus_bit(uint32_t mask) — clear bits in sstatus */
    .globl clear_sstatus_bit
clear_sstatus_bit:
    csrc 0x100, a0     /* sstatus &= ~a0 */
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
 * void set_mpp_supervisor(void)
 *
 * Set mstatus.MPP to Supervisor (01) and MPIE to 1.
 * After mret, CPU enters S-mode with interrupts enabled.
 */
    .globl set_mpp_supervisor
set_mpp_supervisor:
    csrr t0, mstatus
    /* Clear MPP bits (11:12) */
    li   t1, ~(3 << 11)
    and  t0, t0, t1
    /* Set MPP = 01 (Supervisor) */
    li   t1, (1 << 11)
    or   t0, t0, t1
    /* Set MPIE = 1 (enable interrupts on mret) */
    li   t1, (1 << 7)
    or   t0, t0, t1
    csrw mstatus, t0
    ret

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
 * void drop_to_smode(uint32_t entry)
 *
 * Set mepc to entry address and execute mret to transition from
 * M-mode to S-mode. Caller must have set mstatus.MPP=Supervisor
 * and mstatus.MPIE=1 beforehand.
 *
 * Never returns.
 */
    .globl drop_to_smode
drop_to_smode:
    csrw mepc, a0
    mret

/*
 * Note on STIP handling:
 * The M-mode timer handler clears and re-sets STIP on each timer tick.
 * The S-mode kernel does NOT need to clear STIP — it's handled automatically
 * by the M-mode handler when the next timer interrupt fires.
 * Between ticks, SIE=0 during S-mode trap handling prevents re-triggering.
 */
