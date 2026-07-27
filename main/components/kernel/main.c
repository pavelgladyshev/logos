/*
 * Kernel main - RISC-V kernel with process model
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Initializes trap handling, mounts filesystem, loads and executes user programs.
 * Each program runs in its own 64KB memory slot with a per-process trap frame.
 */

#include "console.h"
#include "fs.h"
#include "fs_types.h"
#include "loader.h"
#include "trap.h"
#include "syscall.h"
#include "process.h"
#include "seed_filesystem.h"

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
        logos_printf("KERNEL: Unexpected interrupt, mcause=0x%x\n", cause);
        trap_ret(tf);
    } else {
        logos_printf("KERNEL: Exception! mcause=%d, mepc=0x%x\n", cause, tf->mepc);
        logos_printf("KERNEL: Halting.\n");
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

    logos_printf("Before first printf\n");
    logos_printf("Welcome to logOS Belfield 1.0!\n\n");
    logos_printf("After first printf\n");

    /* Initialize subsystems */
    console_dev_init();
    proc_init();

    /* Initialize flash before trying to read the filesystem superblock. */
    result = block_init();
    if (result != FS_OK) {
        logos_printf("ERROR: block_init failed with code %d\n", result);
        logos_printf("Halting.\n");
        while (1) {}
    }

    /* Preserve an existing filesystem; format only uninitialized storage. */
    logos_printf("Mounting filesystem...\n");

    /////////
    //had to do this because somehow the file system had gotten corrupted and would not mount
    //uint32_t blocks = (uint32_t)block_count();
    //result = fs_format(blocks);
    /////////
    result = fs_mount();
    if (result != FS_OK) {
        uint32_t blocks = (uint32_t)block_count();

        logos_printf("No valid filesystem, formatting %u blocks...\n", blocks);
        result = fs_format(blocks);
        if (result != FS_OK) {
            logos_printf("ERROR: fs_format failed with code %d\n", result);
            logos_printf("Halting.\n");
            while (1) {}
        }

        result = fs_mount();
        if (result != FS_OK) {
            logos_printf("ERROR: fs_mount after format failed with code %d\n", result);
            logos_printf("Halting.\n");
            while (1) {}
        }
    }

    logos_printf("seeding file system\n");
    /* Install built-in programs if this filesystem predates their addition. */
    result = logos_seed_filesystem();
    if (result != FS_OK) {
        logos_printf("ERROR: filesystem seeding failed with code %d\n", result);
        logos_printf("Halting.\n");
        while (1) {}
    }

    logos_printf("Filesystem mounted.\n\n");
    logos_printf("Starting shell\n");
    logos_printf("Type 'help' for available commands\n\n");

    /* Run shell in a loop - restarts after each shell exit */
    for (;;) {
        int slot = proc_alloc();
        struct process *p;

        if (slot < 0) {
            logos_printf("ERROR: No free process slot\n");
            logos_printf("Halting.\n");
            while (1) {}
        }
        p = &proc_table[slot];

        /* Initialize per-process environment with defaults */
        proc_env_init(slot);
        proc_set_env_int(slot, "?", last_exit_code);

        /* Load shell into this process slot */
        result = elf_load_at("/bin/sh", p->mem_base, PROC_SLOT_SIZE, &info);
        if (result != LOAD_OK) {
        logos_printf("ERROR: Failed to load /bin/sh (code %d)\n", result);
        logos_printf("Halting.\n");
        proc_free(slot);
        while (1) {}
        }

        logos_printf(
              "PROC: slot=%d base=0x%x entry=0x%x stack=0x%x first=0x%x\n",
              slot,
              p->mem_base,
              info.entry_point,
              p->stack_top,
              *(uint32_t *)p->mem_base
          );
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
        logos_printf("PROC: installing trap handler\n");
        set_trap_handler(kernel_vector_table, &p->tf);
        logos_printf("PROC: entering user program\n");
        run_user_program(&p->tf);
        logos_printf("PROC: user program returned\n");

        /* Shell exited — save exit code for next shell instance */
        last_exit_code = p->exit_code;
        logos_printf("\n[ exit() code %d, reloading shell... ]\n\n", last_exit_code);
        proc_free(slot);
    }

    return 0;
}
