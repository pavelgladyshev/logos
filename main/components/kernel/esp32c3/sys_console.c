#include "syscall.h" 
#include <stddef.h>

#include "console.h"

/* Console syscalls are thin wrappers around the kernel console backend. */
int sys_getchar(void){
    return logos_getchar();
}


int sys_putchar(int ch){
    logos_putchar((char)ch);
    return ch;
}

int sys_write(int fd, const void *buf, int len){

    const unsigned char* s = buf;

    /* Only stdout and stderr are supported by the early console layer. */
    if(fd != 1 && fd != 2){
        return -1;
    }
    
    if(buf == NULL){
        return -1;
    }

    if(len < 0){
        return -1;
    }

    return logos_write(s, len);
}
