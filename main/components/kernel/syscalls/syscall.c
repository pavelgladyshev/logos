#include "syscall.h"

/* Dispatch machine-mode ecall requests using the saved trap frame registers. */
void syscall_dispatch(trap_frame_t *tf){
    switch (tf->a7) {
        case SYS_PUTCHAR:
            tf->a0 = sys_putchar((int)tf->a0);
            break;
        case SYS_GETCHAR:
            tf->a0 = sys_getchar();
            break;
        case SYS_WRITE:
            tf->a0 = sys_write((int)tf->a0, (const void *)tf->a1, (int)tf->a2);
            break;
        default:
            tf->a0 = -1;
            break;
    }
}

