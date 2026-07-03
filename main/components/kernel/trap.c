#include <stdint.h>

#include "console.h"
#include "riscv/rv_utils.h"
#include "syscalls/syscall.h"
#include "trap.h"

#define MCAUSE_ECALL_M_MODE 11
#define TRAP_STACK_SIZE 2048

extern void kernel_vector_table(void);

/* Dedicated machine-mode trap stack. mscratch points at its top between traps. */
static uint8_t s_trap_stack[TRAP_STACK_SIZE] __attribute__((aligned(16)));

void trap_install(void)
{
    uintptr_t trap_stack_top = (uintptr_t)&s_trap_stack[TRAP_STACK_SIZE];

    rv_utils_intr_global_disable();
    /* trap.S swaps sp with mscratch on entry, so mscratch must hold a stack. */
    asm volatile ("csrw mscratch, %0" :: "r"(trap_stack_top) : "memory");
    rv_utils_set_xtvec((uint32_t)&kernel_vector_table);
}

void c_trap_handler(trap_frame_t* tf){
    /* Current userspace/syscall shim runs in machine mode, so ecall is cause 11. */
    if(tf->mcause == MCAUSE_ECALL_M_MODE){
        syscall_dispatch(tf);

        /* Resume after the 4-byte ecall instruction. */
        tf->mepc += 4;
        return;
    }

    /* Anything else is fatal until interrupt/fault handling is expanded. */
    kernel_console_printf("Unhandled trap: mcause=%lu mepc=0x%08lx ra=0x%08lx mtval=0x%08lx\n",
                          (unsigned long)tf->mcause,
                          (unsigned long)tf->mepc,
                          (unsigned long)tf->ra,
                          (unsigned long)tf->mtval);
    while(1){

    }
}
