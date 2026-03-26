/*
 * SV32 Virtual Memory Management
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Provides page table allocation, mapping, and virtual-to-physical
 * address translation for RISC-V SV32 (2-level, 4KB pages).
 */

#ifndef VM_H
#define VM_H

#include "types.h"

/* ============================
 * SV32 Constants
 * ============================ */

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define PAGES(bytes)    (((bytes) + PAGE_SIZE - 1) / PAGE_SIZE)

/* PTE flag bits */
#define PTE_V   (1 << 0)   /* Valid */
#define PTE_R   (1 << 1)   /* Read */
#define PTE_W   (1 << 2)   /* Write */
#define PTE_X   (1 << 3)   /* Execute */
#define PTE_U   (1 << 4)   /* User-accessible */
#define PTE_G   (1 << 5)   /* Global */
#define PTE_A   (1 << 6)   /* Accessed */
#define PTE_D   (1 << 7)   /* Dirty */

/* Convenience flag combos */
#define PTE_KERN_RWX    (PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D)
#define PTE_KERN_RW     (PTE_V | PTE_R | PTE_W | PTE_A | PTE_D)
#define PTE_USER_RWX    (PTE_V | PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D)
#define PTE_USER_RW     (PTE_V | PTE_R | PTE_W | PTE_U | PTE_A | PTE_D)

/* PTE manipulation */
#define PTE_FLAGS(pte)      ((pte) & 0x3FF)
#define PTE_PPN(pte)        (((pte) >> 10) & 0x3FFFFF)
#define PTE_PPN1(pte)       (((pte) >> 20) & 0x3FF)
#define PTE_PPN0(pte)       (((pte) >> 10) & 0x3FF)
#define PA_TO_PPN(pa)       ((uint32_t)(pa) >> PAGE_SHIFT)
#define PPN_TO_PA(ppn)      ((uint32_t)(ppn) << PAGE_SHIFT)
#define MAKE_PTE(ppn, flags) (((uint32_t)(ppn) << 10) | (flags))

/* Virtual address decomposition (SV32: 10+10+12) */
#define VA_VPN1(va)     (((uint32_t)(va) >> 22) & 0x3FF)
#define VA_VPN0(va)     (((uint32_t)(va) >> 12) & 0x3FF)
#define VA_OFFSET(va)   ((uint32_t)(va) & 0xFFF)

/* SATP register */
#define SATP_SV32_MODE  (1U << 31)
#define MAKE_SATP(mode, asid, ppn) \
    (((uint32_t)(mode) << 31) | ((uint32_t)(asid) << 22) | (uint32_t)(ppn))

/* ============================
 * Page Table Pool
 * ============================ */

/* Physical region reserved for page tables: 128KB = 32 pages of 4KB */
#define PT_POOL_BASE    0x00158000
#define PT_POOL_PAGES   32
#define PT_POOL_END     (PT_POOL_BASE + PT_POOL_PAGES * PAGE_SIZE)

/* ============================
 * Virtual Address Layout
 * ============================ */

#define USER_VA_BASE    0x00400000
#define USER_VA_END     (USER_VA_BASE + 0x8000)     /* 32KB */
#define USER_STACK_TOP  (USER_VA_BASE + 0x8000 - 0x100)

#define SHM_VA_BASE     0x00500000

/* ============================
 * Functions
 * ============================ */

/*
 * Initialize the page table pool (zero all pages, clear bitmap).
 */
void vm_init(void);

/*
 * Allocate a zeroed 4KB page from the pool.
 * Returns physical address, or 0 on failure.
 */
uint32_t pt_alloc(void);

/*
 * Free a page back to the pool.
 */
void pt_free(uint32_t pa);

/*
 * Free all page tables associated with a process slot.
 * Walks the root page table to find and free L0 tables, then frees root.
 */
void pt_free_all(uint32_t root_pa);

/*
 * Map a single 4KB page: va -> pa with given flags.
 * Allocates L0 page table if needed.
 * Returns 0 on success, -1 on failure.
 */
int map_page(uint32_t root_pa, uint32_t va, uint32_t pa, uint32_t flags);

/*
 * Map a 4MB megapage (L1 leaf entry): va -> pa with given flags.
 * va and pa must be 4MB-aligned.
 */
int map_megapage(uint32_t root_pa, uint32_t va, uint32_t pa, uint32_t flags);

/*
 * Map a contiguous range of 4KB pages.
 * Returns 0 on success, -1 on failure.
 */
int map_range(uint32_t root_pa, uint32_t va_start, uint32_t pa_start,
              uint32_t size, uint32_t flags);

/*
 * Unmap a single 4KB page (clear the L0 PTE).
 */
void unmap_page(uint32_t root_pa, uint32_t va);

/*
 * Build kernel identity mappings in a root page table.
 * Maps kernel, process slots, SHM, PT pool, and MMIO regions
 * WITHOUT the U bit (supervisor-only access).
 */
void build_kernel_mappings(uint32_t root_pa);

/*
 * Build user-space mappings for a process slot.
 * Maps PROC_SLOT_BASE(slot) to USER_VA_BASE with U bit.
 */
void build_user_mappings(uint32_t root_pa, int slot);

/*
 * Set up identity-mapped page tables for a process slot.
 * Allocates root PT, builds kernel + user mappings, sets p->satp.
 * Returns 0 on success, -1 on failure.
 */
int setup_process_vm(int slot);

/*
 * Translate a user virtual address to physical address.
 * Uses simple arithmetic (known fixed mappings).
 * Returns physical address, or 0 if invalid.
 */
uint32_t uva_to_pa(int slot, uint32_t uva);

/*
 * Copy data from user VA to kernel buffer.
 * Returns number of bytes copied, or -1 on error.
 */
int copyin(int slot, void *kbuf, uint32_t uva, uint32_t len);

/*
 * Copy data from kernel buffer to user VA.
 * Returns number of bytes copied, or -1 on error.
 */
int copyout(int slot, uint32_t uva, const void *kbuf, uint32_t len);

/*
 * Copy null-terminated string from user VA to kernel buffer.
 * Returns string length, or -1 on error.
 */
int copyinstr(int slot, char *kbuf, uint32_t uva, uint32_t maxlen);

/* ============================
 * CSR wrappers (in trap.S / m_trap.S)
 * ============================ */

/* S-mode CSR accessors */
void write_satp(uint32_t val);
uint32_t read_satp(void);
void sfence_vma(void);
void write_stvec(uint32_t val);
void write_sscratch(uint32_t val);
uint32_t read_scause(void);
uint32_t read_stval(void);
uint32_t read_sepc(void);
void write_sepc(uint32_t val);

/* Privilege mode helpers */
void set_mpp_supervisor(void);  /* Set mstatus.MPP = Supervisor (01) */
void clear_spp(void);           /* Clear sstatus.SPP = User (0) */
void set_spie(void);            /* Set sstatus.SPIE = 1 */
void clear_stip(void);          /* Clear SIP.STIP to acknowledge S-mode timer */
void write_sie(uint32_t val);   /* Write supervisor interrupt enable */
void set_sstatus_bit(uint32_t mask);
void clear_sstatus_bit(uint32_t mask);

/* M-mode setup */
void set_mtvec(uint32_t handler);  /* Write M-mode trap vector */

/* M-mode delegation setup */
void write_medeleg(uint32_t val);
void write_mideleg(uint32_t val);

/* Drop from M-mode to S-mode. Sets mepc and executes mret. Never returns. */
void drop_to_smode(uint32_t entry) __attribute__((noreturn));

#endif /* VM_H */
