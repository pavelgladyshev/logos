/*
 * trap_frame data structure. Used for saving main program context when handling interrupts and exceptions
 */

#pragma once

typedef unsigned int uint32;      // data type big enough to hold 32 bit integers

#define MCAUSE_INTERRUPT ((uint32)0x80000000)   
#define MCAUSE_EXTERNAL  ((uint32)0xb)
#define MCAUSE_TIMER     ((uint32)0x7)

struct trap_frame {
  /*   0 */ uint32 reserved1;          
  /*   4 */ uint32 c_trap_sp;     // top of program stack for C trap handler c_trap()
  /*   8 */ uint32 c_trap;        // pointer to c_trap() function handling interrupts in C
  /*  12 */ uint32 mepc;           
  /*  16 */ uint32 reserved3; 
  /*  20 */ uint32 ra;
  /*  24 */ uint32 sp;
  /*  28 */ uint32 gp;
  /*  32 */ uint32 tp;
  /*  36 */ uint32 t0;
  /*  40 */ uint32 t1;
  /*  44 */ uint32 t2;
  /*  48 */ uint32 s0;
  /*  52 */ uint32 s1;
  /*  56 */ uint32 a0;
  /*  60 */ uint32 a1;
  /*  64 */ uint32 a2;
  /*  68 */ uint32 a3;
  /*  72 */ uint32 a4;
  /*  76 */ uint32 a5;
  /*  80 */ uint32 a6;
  /*  84 */ uint32 a7;
  /*  88 */ uint32 s2;
  /*  92 */ uint32 s3;
  /*  96 */ uint32 s4;
  /* 100 */ uint32 s5;
  /* 104 */ uint32 s6;
  /* 108 */ uint32 s7;
  /* 112 */ uint32 s8;
  /* 116 */ uint32 s9;
  /* 120 */ uint32 s10;
  /* 124 */ uint32 s11;
  /* 128 */ uint32 t3;
  /* 132 */ uint32 t4;
  /* 136 */ uint32 t5;
  /* 140 */ uint32 t6;
};

typedef struct trap_frame trap_frame_t;

/* writes address of the asembly language trap_handler routine into CPU's mtvec register 
 * and writes the address of the trap_frame structure into CPU's mscratch register 
 */
void set_trap_handler(void (*trap_handler)(trap_frame_t *),trap_frame_t *trap_frame);

/* Functions to read and write mie Status and Control Register from C program*/
void set_mie(uint32 mie_value);
uint32 get_mie();

/* set bits in mstatus register */
void set_mstatus_bit(uint32 mask);

/* Functions to globally enable / disable trap handling */
void enable_interrupts();
void disable_interrupts();

/* 
 * Assembly language interrupt and exception service routine, saves CPU registers, 
 * sets-up C program stack for C language interrrupt handler and calls trap_frame->c_trap()
 */
void trap_handler();

/* Get value of CPU's mcause register that indicates the reason for the trap*/
uint32 get_mcause();

/*
 * Assembly language routine for restoring main program context and returning to it.
 * Called from C language trap handler, never returns.
 */
void trap_ret(trap_frame_t *trap_frame);