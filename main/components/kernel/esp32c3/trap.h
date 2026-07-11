#ifndef TRAP_HANDLER
#define TRAP_HANDLER

#include <stdint.h>

/* Saved machine context layout. Offsets must match trap.S exactly. */
typedef struct trap_frame{

    uint32_t reserved;
    uint32_t c_trap_sp;
    uint32_t c_trap;
    uint32_t mepc;
    uint32_t mstatus;
    uint32_t ra;
    uint32_t sp;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t s0;
    uint32_t s1;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;

    uint32_t mcause;
    uint32_t mtval;


} trap_frame_t ;


void trap_install(void);
void c_trap_handler(trap_frame_t *tf);
void set_trap_handler(void (*handler)(void), trap_frame_t *tf);
void trap_handler(void);
void trap_ret(trap_frame_t *tf);
int run_user_program(trap_frame_t *tf);
uint32_t get_mcause(void);
void set_mie(uint32_t mie_value);
uint32_t get_mie(void);
void set_mstatus_bit(uint32_t mask);
void enable_interrupts(void);
void disable_interrupts(void);

#endif /* ifndef TRAP_HANDLER
#define TRAP_HANDLER

#include <stdint.h> */
