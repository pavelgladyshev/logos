/*
 * Trap frame and interrupt handling declarations
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef TRAP_H
#define TRAP_H

#include "types.h"

/* Machine cause register values */
#define MCAUSE_INTERRUPT  ((uint32_t)0x80000000)  /* Interrupt flag (bit 31) */
#define MCAUSE_EXTERNAL   ((uint32_t)0xb)         /* External interrupt */
#define MCAUSE_TIMER      ((uint32_t)0x7)         /* Timer interrupt */
#define MCAUSE_ECALL      ((uint32_t)0xb)         /* Environment call from M-mode */

/*
 * Trap frame structure - saves complete CPU context during traps.
 * Used for saving program context when handling interrupts, exceptions, and syscalls.
 */
struct trap_frame {
    /*   0 */ uint32_t reserved1;
    /*   4 */ uint32_t c_trap_sp;    /* Stack pointer for C trap handler */
    /*   8 */ uint32_t c_trap;       /* Pointer to C trap handler function */
    /*  12 */ uint32_t mepc;         /* Machine exception program counter */
    /*  16 */ uint32_t reserved3;
    /*  20 */ uint32_t ra;           /* Return address */
    /*  24 */ uint32_t sp;           /* Stack pointer (user program) */
    /*  28 */ uint32_t gp;           /* Global pointer */
    /*  32 */ uint32_t tp;           /* Thread pointer */
    /*  36 */ uint32_t t0;           /* Temporary registers */
    /*  40 */ uint32_t t1;
    /*  44 */ uint32_t t2;
    /*  48 */ uint32_t s0;           /* Saved registers */
    /*  52 */ uint32_t s1;
    /*  56 */ uint32_t a0;           /* Argument/return registers */
    /*  60 */ uint32_t a1;
    /*  64 */ uint32_t a2;
    /*  68 */ uint32_t a3;
    /*  72 */ uint32_t a4;
    /*  76 */ uint32_t a5;
    /*  80 */ uint32_t a6;
    /*  84 */ uint32_t a7;           /* Syscall number */
    /*  88 */ uint32_t s2;           /* More saved registers */
    /*  92 */ uint32_t s3;
    /*  96 */ uint32_t s4;
    /* 100 */ uint32_t s5;
    /* 104 */ uint32_t s6;
    /* 108 */ uint32_t s7;
    /* 112 */ uint32_t s8;
    /* 116 */ uint32_t s9;
    /* 120 */ uint32_t s10;
    /* 124 */ uint32_t s11;
    /* 128 */ uint32_t t3;           /* More temporary registers */
    /* 132 */ uint32_t t4;
    /* 136 */ uint32_t t5;
    /* 140 */ uint32_t t6;
};

typedef struct trap_frame trap_frame_t;

/*
 * Set up trap handler.
 * Writes address of assembly trap_handler routine into CPU's mtvec register
 * and writes address of trap_frame structure into CPU's mscratch register.
 */
void set_trap_handler(void (*handler)(void), trap_frame_t *tf);

/*
 * Read/write machine interrupt enable register (mie).
 */
void set_mie(uint32_t mie_value);
uint32_t get_mie(void);

/*
 * Set bits in mstatus register.
 */
void set_mstatus_bit(uint32_t mask);

/*
 * Globally enable/disable trap handling.
 */
void enable_interrupts(void);
void disable_interrupts(void);

/*
 * Assembly language trap handler entry point.
 * Saves CPU registers, sets up C stack, and calls c_trap_handler().
 */
void trap_handler(void);

/*
 * Get value of mcause register (reason for trap).
 */
uint32_t get_mcause(void);

/*
 * Assembly routine to restore context and return from trap.
 * Called from C trap handler, never returns.
 */
void trap_ret(trap_frame_t *tf);

/*
 * C language trap handler - called from assembly trap_handler.
 * Dispatches to syscall handler or handles interrupts.
 */
void c_trap_handler(trap_frame_t *tf);

#endif /* TRAP_H */
