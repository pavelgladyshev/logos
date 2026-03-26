/*
 * Kernel main - RISC-V kernel with SV32 virtual memory
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Boot sequence:
 *   1. crt0.S entry (M-mode): set sp, zero BSS, call main()
 *   2. main() (M-mode): init subsystems, set up delegation, create
 *      kernel page tables, write satp, drop to S-mode
 *   3. s_mode_main() (S-mode): shell restart loop with SV32 paging
 *
 * Trap handling:
 *   - M-mode: timer interrupts only (m_trap.S forwards as STIP)
 *   - S-mode: ecall from U-mode, supervisor timer, page faults
 */

#include "fs.h"
#include "console.h"
#include "console_dev.h"
#include "loader.h"
#include "trap.h"
#include "syscall.h"
#include "process.h"
#include "vm.h"

/* Kernel trap stack - used when handling traps from user programs */
#define TRAP_STACK_SIZE 4096
static uint8_t trap_stack[TRAP_STACK_SIZE] __attribute__((aligned(16)));

/* Timer MMIO registers (also used by M-mode handler in m_trap.S) */
#define TIMER_MTIME    ((volatile uint32_t *)0x200bff8)
#define TIMER_MTIMECMP ((volatile uint32_t *)0x2004000)
#define TIME_SLICE     800

/* Flag to indicate the current top-level program has exited */
volatile int program_should_exit = 0;

/* Assembly function to save kernel context and jump to user program */
extern int run_user_program(trap_frame_t *tf);

/* M-mode trap handler (timer forwarder, in m_trap.S) */
extern void m_trap_handler(void);

/* Supervisor-mode trap cause values */
#define SCAUSE_INTERRUPT       ((uint32_t)0x80000000)
#define SCAUSE_S_SW_INT        ((uint32_t)(SCAUSE_INTERRUPT | 1))
#define SCAUSE_S_TIMER_INT     ((uint32_t)(SCAUSE_INTERRUPT | 5))
#define SCAUSE_ECALL_FROM_U    ((uint32_t)8)
#define SCAUSE_INST_PAGE_FAULT ((uint32_t)12)
#define SCAUSE_LOAD_PAGE_FAULT ((uint32_t)13)
#define SCAUSE_STORE_PAGE_FAULT ((uint32_t)15)

/* Delegation bitmasks */
/* Delegate: ecall-from-U(8), inst page fault(12), load page fault(13),
 * store page fault(15), illegal instruction(2), breakpoint(3),
 * load/store address misaligned(4,6), load/store access fault(5,7) */
#define MEDELEG_BITS  ((1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8)|(1<<12)|(1<<13)|(1<<15))
/* Delegate supervisor software interrupt (bit 1), supervisor timer (bit 5),
 * and supervisor external (bit 9).
 * Timer forwarding uses SSIP (bit 1) because STIP is read-only from S-mode. */
#define MIDELEG_BITS  ((1<<1)|(1<<5)|(1<<9))

/* Root page table for kernel identity mapping (shared by all processes in Phase 2) */
static uint32_t kernel_pt_root;

/*
 * C language trap handler - called from assembly trap_handler (S-mode).
 * Dispatches system calls and handles other traps.
 */
void c_trap_handler(trap_frame_t *tf) {
    uint32_t cause = get_mcause();  /* Actually reads scause (wrapper renamed) */

    if (cause == SCAUSE_ECALL_FROM_U) {
        /* System call from user mode */
        if (syscall_dispatch(tf)) {
            /* sys_exit was called by a process with no parent.
             * Return to run_user_program via restore_kernel_context. */
            program_should_exit = 1;
            return;
        }
        /* Normal syscall - return to user program */
        trap_ret(tf);
    } else if (cause == SCAUSE_S_SW_INT) {
        /* Supervisor software interrupt — used for timer forwarding from M-mode.
         * Clear SSIP (writable from S-mode) to prevent re-entry after sret. */
        clear_ssip();
        proc_table[current_proc].state = PROC_READY;
        schedule();  /* never returns */
    } else if (cause == SCAUSE_S_TIMER_INT) {
        /* Direct supervisor timer — clear and reschedule */
        clear_stip();
        proc_table[current_proc].state = PROC_READY;
        schedule();  /* never returns */
    } else if (cause & SCAUSE_INTERRUPT) {
        printf("KERNEL: Unexpected S-mode interrupt, scause=0x%x\n", cause);
        trap_ret(tf);
    } else if (cause == SCAUSE_INST_PAGE_FAULT ||
               cause == SCAUSE_LOAD_PAGE_FAULT ||
               cause == SCAUSE_STORE_PAGE_FAULT) {
        uint32_t faulting_va = read_stval();
        printf("KERNEL: Page fault! scause=%d va=0x%x pid=%d (%s) sepc=0x%x\n",
               cause, faulting_va, proc_table[current_proc].pid,
               proc_table[current_proc].name, tf->mepc);
        /* Kill the faulting process */
        proc_table[current_proc].exit_code = -1;
        proc_table[current_proc].state = PROC_ZOMBIE;
        /* Wake parent if waiting */
        if (proc_table[current_proc].parent >= 0) {
            int parent = proc_table[current_proc].parent;
            if (proc_table[parent].state == PROC_SLEEPING) {
                proc_table[parent].state = PROC_READY;
            }
        }
        schedule();  /* Switch to another process */
    } else {
        printf("KERNEL: Exception! scause=%d, sepc=0x%x\n", cause, tf->mepc);
        printf("KERNEL: stval=0x%x\n", read_stval());
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

/*
 * S-mode main — entered after M-mode drops privilege.
 * Runs the shell in a loop, restarting after each exit.
 */
static void s_mode_main(void) {
    int result;
    struct program_info info;
    int last_exit_code = 0;

    printf("Starting shell\n");
    printf("Type 'help' for available commands\n\n");

    for (;;) {
        int slot;
        struct process *p;

        slot = proc_alloc();
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
        /* Set process name */
        {
            const char *n = "sh";
            int i;
            for (i = 0; n[i]; i++) p->name[i] = n[i];
            p->name[i] = '\0';
        }

        /* Set up process state */
        setup_trap_frame(slot);
        setup_process_vm(slot);
        /* Use virtual addresses — entry point is offset from slot base, mapped at USER_VA_BASE */
        p->tf.mepc = USER_VA_BASE + (info.entry_point - p->mem_base);
        p->tf.ra = 0;
        p->tf.sp = USER_STACK_TOP;
        p->tf.a0 = 0;  /* argc = 0 */
        p->tf.a1 = 0;  /* argv = NULL */
        p->state = PROC_RUNNING;
        p->parent = -1;  /* No parent — kernel manages shell */
        proc_fd_init(slot);

        /* Activate this process's page tables and prepare for U-mode */
        current_proc = slot;
        program_should_exit = 0;
        write_satp(p->satp);
        sfence_vma();
        clear_spp();   /* SPP=0 so sret drops to U-mode */
        set_spie();    /* SPIE=1 so sret enables interrupts */
        set_trap_handler(trap_handler, &p->tf);
        run_user_program(&p->tf);

        /* Shell exited — save exit code for next shell instance */
        last_exit_code = p->exit_code;
        printf("\n[ exit() code %d, reloading shell... ]\n\n", last_exit_code);
        proc_free(slot);
    }
}

int main(void)
{
    int result;

    /* Initialize subsystems (still in M-mode) */
    console_dev_init();
    proc_init();
    vm_init();

    result = fs_mount();
    if (result != FS_OK) {
        printf("ERROR: fs_mount failed (%d)\n", result);
        return result;
    }

    /* === Set up M-mode delegation and drop to S-mode === */

    /* 1. Delegate exceptions and interrupts to S-mode */
    write_medeleg(MEDELEG_BITS);
    write_mideleg(MIDELEG_BITS);

    /* 2. Install M-mode timer forwarder (stays in mtvec permanently) */
    set_mtvec((uint32_t)m_trap_handler);

    /* 3. Enable M-mode timer interrupt */
    set_mie(0x80);   /* MIE.MTIE (bit 7) */

    /* 4. Arm the first timer tick */
    *TIMER_MTIMECMP = *TIMER_MTIME + TIME_SLICE;

    /* 5. Enable supervisor timer interrupt in SIE */
    write_sie(0x22);  /* SIE.SSIE (bit 1) + SIE.STIE (bit 5) */

    /* 6. Build kernel identity-mapped page tables */
    kernel_pt_root = pt_alloc();
    build_kernel_mappings(kernel_pt_root);

    /* 7. Enable SV32 paging */
    write_satp(MAKE_SATP(1, 0, PA_TO_PPN(kernel_pt_root)));
    sfence_vma();

    /* 8. Drop to S-mode with interrupts DISABLED.
     * Set MPP=Supervisor, MPIE=0 so that mret clears SIE.
     * s_mode_main will enable interrupts after setting up the trap handler. */
    /* Drop to S-mode */
    {
        /* Clear MPP, set to Supervisor (01). Keep MPIE=0. */
        uint32_t mstatus;
        __asm__ volatile ("csrr %0, mstatus" : "=r"(mstatus));
        mstatus &= ~(3 << 11);   /* Clear MPP */
        mstatus |= (1 << 11);    /* Set MPP = Supervisor (01) */
        mstatus &= ~(1 << 7);    /* Clear MPIE → SIE=0 after mret */
        __asm__ volatile ("csrw mstatus, %0" :: "r"(mstatus));
    }

    /* Set mepc to s_mode_main and execute mret to drop to S-mode */
    drop_to_smode((uint32_t)s_mode_main);

    /* Never reached */
    return 0;
}
