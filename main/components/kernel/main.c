/*
 * Kernel main - RISC-V kernel with process model
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Initializes trap handling, mounts filesystem, loads and executes user programs.
 * Each program runs in its own 64KB memory slot with a per-process trap frame.
 */

#include "fs.h"
#include "console.h"
#include "console_dev.h"
#include "loader.h"
#include "trap.h"
#include "syscall.h"
#include "process.h"

/* Kernel trap stack - used when handling traps from user programs */
#define TRAP_STACK_SIZE 4096
static uint8_t trap_stack[TRAP_STACK_SIZE] __attribute__((aligned(16)));

/* Flag to indicate the current top-level program has exited */
volatile int program_should_exit = 0;

/* Assembly function to save kernel context and jump to user program */
extern int run_user_program(trap_frame_t *tf);

/*
 * C language trap handler - called from assembly trap_handler.
 * Dispatches system calls and handles other traps.
 *
 * Note: sys_spawn and sys_exit may call trap_ret() directly
 * (never returning here) to switch between processes.
 */
void c_trap_handler(trap_frame_t *tf) {
    uint32_t cause = get_mcause();

    if (cause == 11) {
        /* System call (ecall) - dispatch it */
        if (syscall_dispatch(tf)) {
            /* sys_exit was called by a process with no parent.
             * Return to run_user_program via restore_kernel_context. */
            program_should_exit = 1;
            return;
        }
        /* Normal syscall - return to user program */
        trap_ret(tf);
    } else if (cause & MCAUSE_INTERRUPT) {
        printf("KERNEL: Unexpected interrupt, mcause=0x%x\n", cause);
        trap_ret(tf);
    } else {
        printf("KERNEL: Exception! mcause=%d, mepc=0x%x\n", cause, tf->mepc);
        printf("KERNEL: Halting.\n");
        while (1) {}
    }
}

/*
 * Set up trap frame kernel fields for a process slot.
 * Must be called before running a process.
 */
static void setup_trap_frame(int slot) {
    struct process *p = &proc_table[slot];
    p->tf.c_trap_sp = (uint32_t)(trap_stack + TRAP_STACK_SIZE);
    p->tf.c_trap = (uint32_t)c_trap_handler;
}

int main(void)
{
    int result;
    struct program_info info;
    int last_exit_code = 0;

    printf("Welcome to logOS Belfield 1.0!\n\n");

    /* Initialize subsystems */
    console_dev_init();
    proc_init();

    /* Mount the filesystem */
    printf("Mounting filesystem...\n");
    result = fs_mount();
    if (result != FS_OK) {
        printf("ERROR: fs_mount failed with code %d\n", result);
        return result;
    }
    printf("Filesystem mounted.\n\n");
    printf("Starting shell\n");
    printf("Type 'help' for available commands\n\n");

    /* Run shell in a loop - restarts after each shell exit */
    for (;;) {
        int slot = proc_alloc();
        struct process *p;

        if (slot < 0) {
            printf("ERROR: No free process slot\n");
            printf("Halting.\n");
            while (1) {}
        }
        p = &proc_table[slot];

        /* Initialize per-process environment with defaults */
        proc_env_init(slot);
        proc_set_env_int(slot, "?", last_exit_code);

        /* Load shell into this process slot */
        result = elf_load_at("/bin/sh", p->mem_base, PROC_SLOT_SIZE, &info);
        if (result != LOAD_OK) {
            printf("ERROR: Failed to load /bin/sh (code %d)\n", result);
            printf("Halting.\n");
            proc_free(slot);
            while (1) {}
        }

        /* Set up process state */
        setup_trap_frame(slot);
        p->tf.mepc = info.entry_point;
        p->tf.ra = 0;
        p->tf.sp = p->stack_top;
        p->tf.a0 = 0;  /* argc = 0 */
        p->tf.a1 = 0;  /* argv = NULL */
        p->state = PROC_RUNNING;
        p->parent = -1;  /* No parent — kernel manages shell */
        proc_fd_init(slot);

        /* Start the process */
        current_proc = slot;
        program_should_exit = 0;
        set_trap_handler(trap_handler, &p->tf);
        run_user_program(&p->tf);

        /* Shell exited — save exit code for next shell instance */
        last_exit_code = p->exit_code;
        printf("\n[ exit() code %d, reloading shell... ]\n\n", last_exit_code);
        proc_free(slot);
    }

    return 0;
}
