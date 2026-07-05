#ifndef SYSCALL_H
#define SYSCALL_H
#include "trap.h"

#define SYS_PUTCHAR 1
#define SYS_GETCHAR 2
#define SYS_WRITE 3

void syscall_dispatch(trap_frame_t *tf);

int sys_putchar(int ch);
int sys_getchar(void);
int sys_write(int fd, const void *buf, int len);

#endif /* ifndef SYSCALL_H */
