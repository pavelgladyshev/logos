#include "user_syscall.h"
#include "esp32c3/syscall.h"
#include <stdint.h>

/* Place syscall number/arguments in the RISC-V ABI registers, then trap. */
static int logos_syscall(int num, uintptr_t a0, uintptr_t a1, uintptr_t a2){

    register uintptr_t x10 asm("a0") = a0;
    register uintptr_t x11 asm("a1") = a1;
    register uintptr_t x12 asm("a2") = a2;
    register uintptr_t x17 asm("a7") = num;

    asm volatile (
            "ecall"
            : "+r"(x10)
            : "r"(x11), "r"(x12), "r"(x17)
            : "memory"
            );

    return (int)x10;
}



/* User-facing console calls. The kernel side implements these in syscall.c. */
int logos_putchar(int ch){
    return logos_syscall(SYS_PUTCHAR, ch, 0, 0);
}

int logos_getchar(void){
    return logos_syscall(SYS_GETCHAR, 0, 0, 0);
}

int logos_write(int fd, const void *buf, int len){
    return logos_syscall(SYS_WRITE, fd, (uintptr_t)buf, len); 
}
