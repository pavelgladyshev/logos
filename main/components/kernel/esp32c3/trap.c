#include <stdint.h>

#include "console.h"
#include "riscv/rv_utils.h"
#include "esp32c3/syscall.h"
#include "trap.h"

#define MCAUSE_ECALL_U_MODE 8
#define MCAUSE_ECALL_M_MODE 11
#define TRAP_STACK_SIZE 4096

/* Fallback frame used before a real process trap frame is installed. */
static trap_frame_t s_kernel_tf;
static uint8_t s_trap_stack[TRAP_STACK_SIZE] __attribute__((aligned(16)));

void trap_install(void)
{
    uintptr_t trap_stack_top = (uintptr_t)&s_trap_stack[TRAP_STACK_SIZE];

    rv_utils_intr_global_disable();

    s_kernel_tf.c_trap_sp = trap_stack_top;
    s_kernel_tf.c_trap = (uint32_t)c_trap_handler;

    set_trap_handler(trap_handler, &s_kernel_tf);
}

void c_trap_handler(trap_frame_t* tf){
      uint32_t cause = get_mcause();
      static int trap_count = 0;

      if (trap_count < 20) {
          logos_printf(
              "TRAP: cause=%u mepc=0x%x a7=%u "
              "a0=0x%x a1=0x%x a2=%u mtval=0x%x\n",
              cause,
              tf->mepc,
              tf->a7,
              tf->a0,
              tf->a1,
              tf->a2,
              tf->mtval
          );
          trap_count++;
      }
    logos_printf("trap mcause=%lu mepc=0x%08lx a7=%lu a0=0x%08lx\n",
                          (unsigned long)tf->mcause,
                          (unsigned long)tf->mepc,
                          (unsigned long)tf->a7,
                          (unsigned long)tf->a0);

    if(tf->mcause == MCAUSE_ECALL_U_MODE || tf->mcause == MCAUSE_ECALL_M_MODE){
        syscall_dispatch(tf);

        /* Resume after the 4-byte ecall instruction. */
        tf->mepc += 4;
        trap_ret(tf);
    }

    /* Anything else is fatal until interrupt/fault handling is expanded. */
    logos_printf("Unhandled trap: mcause=%lu mepc=0x%08lx ra=0x%08lx mtval=0x%08lx\n",
                          (unsigned long)tf->mcause,
                          (unsigned long)tf->mepc,
                          (unsigned long)tf->ra,
                          (unsigned long)tf->mtval);
    while(1){

    }
}
