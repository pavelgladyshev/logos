/*
 * Kernel main - RISC-V Bootloader with System Calls
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Initializes trap handling, mounts filesystem, loads and executes user programs.
 * User programs communicate with the kernel via system calls (ecall instruction).
 */

#include "fs.h"
#include "console.h"
#include "console_dev.h"
#include "loader.h"
#include "trap.h"
#include "syscall.h"

/* Kernel trap stack - used when handling traps */
#define TRAP_STACK_SIZE 4096
static uint8_t trap_stack[TRAP_STACK_SIZE] __attribute__((aligned(16)));

/* Global trap frame for the current program */
static trap_frame_t current_tf;

/* Flag to indicate program has exited */
static volatile int program_should_exit = 0;

/* Assembly function to save kernel context and jump to user program */
extern int run_user_program(trap_frame_t *tf);

/*
 * C language trap handler - called from assembly trap_handler.
 * Dispatches system calls and handles other traps.
 */
void c_trap_handler(trap_frame_t *tf) {
    uint32_t cause = get_mcause();

    /* Check if this is an environment call (ecall) from M-mode */
    /* mcause = 11 for ecall from M-mode (we run everything in M-mode) */
    if (cause == 11) {
        /* System call - dispatch it */
        if (syscall_dispatch(tf)) {
            /* sys_exit was called - program wants to terminate */
            program_should_exit = 1;
            /* Return to run_user_program by restoring kernel stack */
            /* This works because we're called from trap_handler which
             * is called via jr from the assembly, not via mret */
            return;
        }
        /* Normal syscall - return to user program */
        trap_ret(tf);
    } else if (cause & MCAUSE_INTERRUPT) {
        /* This is an interrupt */
        printf("KERNEL: Unexpected interrupt, mcause=0x%x\n", cause);
        trap_ret(tf);
    } else {
        /* This is an exception (not ecall) */
        printf("KERNEL: Exception! mcause=%d, mepc=0x%x\n", cause, tf->mepc);
        printf("KERNEL: Halting.\n");
        /* Hang - we don't know how to handle this */
        while (1) {}
    }
}

/*
 * Initialize trap handling infrastructure.
 */
static void trap_init(void) {
    /* Set up the trap frame */
    current_tf.c_trap_sp = (uint32_t)(trap_stack + TRAP_STACK_SIZE);
    current_tf.c_trap = (uint32_t)c_trap_handler;

    /* Install the trap handler */
    set_trap_handler(trap_handler, &current_tf);
}

/*
 * Execute a loaded program with syscall support.
 * Environment is accessed by programs via kernel syscalls.
 * Returns the program's exit code.
 */
static int exec_with_syscalls(struct program_info *info) {
    /* Reset syscall state for new program */
    syscall_init();
    program_should_exit = 0;

    /* Set up the trap frame for the user program */
    current_tf.mepc = info->entry_point;
    current_tf.ra = 0;  /* No return address - program must use exit syscall */
    current_tf.sp = PROGRAM_STACK_TOP;
    current_tf.a0 = 0;  /* argc = 0 */
    current_tf.a1 = 0;  /* argv = NULL */

    /* Run user program - saves kernel context, calls trap_ret,
     * returns when sys_exit is called */
    run_user_program(&current_tf);

    return syscall_get_exit_code();
}

int main(void)
{
    int result;
    struct program_info info;

    printf("Welcome to LogOS!\n\n");

    /* Initialize console device driver */
    console_dev_init();

    /* Initialize trap handling */
    trap_init();

    /* Initialize kernel environment with defaults */
    syscall_env_init();

    /* Mount the filesystem */
    printf("Mounting filesystem...\n");
    result = fs_mount();
    if (result != FS_OK) {
        printf("ERROR: fs_mount failed with code %d\n", result);
        return result;
    }
    printf("Filesystem mounted.\n\n");
    printf("Starting RISC-V Shell\n");
    printf("Type 'help' for available commands\n\n");


    /* Run shell in a loop - restarts after each program exits */
    for (;;) {
        result = elf_load("/bin/sh", &info);
        if (result != LOAD_OK) {
            printf("ERROR: Failed to load /bin/sh (code %d)\n", result);
            printf("Halting.\n");
            while (1) {}
        }

        result = exec_with_syscalls(&info);

        /* Store exit code in ? environment variable */
        {
            char code_str[12];
            int val = result;
            int neg = 0;
            int pos = 0;
            int i;

            if (val < 0) { neg = 1; val = -val; }
            do {
                code_str[pos++] = '0' + (val % 10);
                val /= 10;
            } while (val > 0);
            if (neg) code_str[pos++] = '-';
            /* Reverse in place */
            for (i = 0; i < pos / 2; i++) {
                char t = code_str[i];
                code_str[i] = code_str[pos - 1 - i];
                code_str[pos - 1 - i] = t;
            }
            code_str[pos] = '\0';
            syscall_set_env("?", code_str);
        }
    }

    return 0;
}
