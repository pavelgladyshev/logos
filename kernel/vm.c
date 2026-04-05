/*
 * SV32 Virtual Memory Management
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Page table allocation, mapping, and address translation for
 * RISC-V SV32 (2-level page tables, 4KB pages, 4MB megapages).
 */

#include "vm.h"
#include "process.h"
#include "shm.h"
#include "string.h"
#include "console.h"

/* ============================
 * Page Table Pool Allocator
 * ============================ */

static uint8_t pt_pool_bitmap[PT_POOL_PAGES];

/* Shared L0 page tables (allocated once, reused by all processes) */
static uint32_t kern_l0_pa;  /* L0 for VPN1=0: kernel identity mappings */
static uint32_t mmio_l0_pa;  /* L0 for VPN1=1023: console MMIO */

void vm_init(void)
{
    int i;
    /* Clear bitmap — pages are zeroed individually by pt_alloc() */
    for (i = 0; i < PT_POOL_PAGES; i++)
        pt_pool_bitmap[i] = 0;
    printf("VM: page table pool at 0x%x (%d pages)\n", PT_POOL_BASE, PT_POOL_PAGES);
}

void vm_build_shared_l0(void)
{
    uint32_t *l0;
    int i;

    /* --- Kernel L0 (VPN1=0): identity-map 0x00000000-0x003FFFFF --- */
    kern_l0_pa = pt_alloc();
    l0 = (uint32_t *)kern_l0_pa;

    /* ROM: 0x00000000-0x0000FFFF (pages 0-15) */
    for (i = 0; i <= 15; i++)
        l0[i] = MAKE_PTE(PA_TO_PPN(i * PAGE_SIZE), PTE_KERN_RWX);

    /* Kernel + process slots + SHM + PT pool: 0x00100000-0x00177FFF (pages 256-375) */
    for (i = 256; i <= 375; i++)
        l0[i] = MAKE_PTE(PA_TO_PPN(i * PAGE_SIZE), PTE_KERN_RWX);

    /* Bootloader BSS + stack: 0x001F0000-0x001FFFFF (pages 496-511) */
    for (i = 496; i <= 511; i++)
        l0[i] = MAKE_PTE(PA_TO_PPN(i * PAGE_SIZE), PTE_KERN_RWX);

    /* Block device MMIO: 0x00200000 (page 512) */
    l0[512] = MAKE_PTE(PA_TO_PPN(512 * PAGE_SIZE), PTE_KERN_RW);

    /* --- MMIO L0 (VPN1=1023): identity-map console at 0xFFFF0xxx --- */
    mmio_l0_pa = pt_alloc();
    l0 = (uint32_t *)mmio_l0_pa;

    /* Console MMIO: 0xFFFF0000 (VPN0 = 1008) */
    l0[1008] = MAKE_PTE(PA_TO_PPN(0xFFFF0000), PTE_KERN_RW);

    printf("VM: shared L0 tables built (kern=0x%x, mmio=0x%x)\n",
           kern_l0_pa, mmio_l0_pa);
}

uint32_t pt_alloc(void)
{
    int i;
    for (i = 0; i < PT_POOL_PAGES; i++) {
        if (!pt_pool_bitmap[i]) {
            pt_pool_bitmap[i] = 1;
            uint32_t pa = PT_POOL_BASE + i * PAGE_SIZE;
            /* Zero page using word writes (4x faster than byte memset) */
            uint32_t *p = (uint32_t *)pa;
            int j;
            for (j = 0; j < PAGE_SIZE / 4; j++)
                p[j] = 0;
            return pa;
        }
    }
    printf("VM: pt_alloc failed - pool exhausted\n");
    return 0;
}

void pt_free(uint32_t pa)
{
    if (pa < PT_POOL_BASE || pa >= PT_POOL_END)
        return;
    int idx = (pa - PT_POOL_BASE) / PAGE_SIZE;
    pt_pool_bitmap[idx] = 0;
}

void pt_free_all(uint32_t root_pa)
{
    uint32_t *root = (uint32_t *)root_pa;
    int i;
    /* Walk L1 entries, free any L0 page tables */
    for (i = 0; i < 1024; i++) {
        uint32_t pte = root[i];
        if (!(pte & PTE_V))
            continue;
        /* If not a leaf (R/W/X all zero), it's a pointer to L0 table */
        if (!(pte & (PTE_R | PTE_W | PTE_X))) {
            uint32_t l0_pa = PPN_TO_PA(PTE_PPN(pte));
            /* Don't free shared kernel/MMIO L0 tables */
            if (l0_pa != kern_l0_pa && l0_pa != mmio_l0_pa)
                pt_free(l0_pa);
        }
    }
    /* Free the root table itself */
    pt_free(root_pa);
}

/* ============================
 * Page Mapping
 * ============================ */

int map_page(uint32_t root_pa, uint32_t va, uint32_t pa, uint32_t flags)
{
    uint32_t *root = (uint32_t *)root_pa;
    uint32_t vpn1 = VA_VPN1(va);
    uint32_t vpn0 = VA_VPN0(va);
    uint32_t l1_pte = root[vpn1];
    uint32_t *l0;

    /* If L1 entry is not valid, allocate an L0 table */
    if (!(l1_pte & PTE_V)) {
        uint32_t l0_pa = pt_alloc();
        if (!l0_pa) return -1;
        root[vpn1] = MAKE_PTE(PA_TO_PPN(l0_pa), PTE_V);
        l0 = (uint32_t *)l0_pa;
    } else {
        /* L1 entry exists — make sure it's not a leaf (megapage) */
        if (l1_pte & (PTE_R | PTE_W | PTE_X)) {
            printf("VM: map_page conflict - L1[%d] is a megapage\n", vpn1);
            return -1;
        }
        l0 = (uint32_t *)PPN_TO_PA(PTE_PPN(l1_pte));
    }

    /* Set the L0 entry */
    l0[vpn0] = MAKE_PTE(PA_TO_PPN(pa), flags);
    return 0;
}

int map_range(uint32_t root_pa, uint32_t va_start, uint32_t pa_start,
              uint32_t size, uint32_t flags)
{
    uint32_t offset;
    for (offset = 0; offset < size; offset += PAGE_SIZE) {
        if (map_page(root_pa, va_start + offset, pa_start + offset, flags) < 0)
            return -1;
    }
    return 0;
}

void unmap_page(uint32_t root_pa, uint32_t va)
{
    uint32_t *root = (uint32_t *)root_pa;
    uint32_t vpn1 = VA_VPN1(va);
    uint32_t vpn0 = VA_VPN0(va);
    uint32_t l1_pte = root[vpn1];

    if (!(l1_pte & PTE_V))
        return;
    if (l1_pte & (PTE_R | PTE_W | PTE_X))
        return;  /* Can't unmap individual page from a megapage */

    uint32_t *l0 = (uint32_t *)PPN_TO_PA(PTE_PPN(l1_pte));
    l0[vpn0] = 0;
}

/* ============================
 * Standard Mappings
 * ============================ */

void build_kernel_mappings(uint32_t root_pa)
{
    /*
     * Install shared L0 page tables for kernel identity mappings.
     * These L0 tables are built once by vm_build_shared_l0() and reused
     * by every process, saving page table pool entries.
     *
     * L1[0] -> kern_l0: ROM, kernel, process slots, SHM, PT pool, block MMIO
     *          Supervisor-only (no U bit). Process slots are also covered
     *          but without U bit — user code accesses its slot via
     *          USER_VA_BASE (different L1 entry) in build_user_mappings().
     *
     * L1[1023] -> mmio_l0: Console MMIO at 0xFFFF0xxx
     */
    uint32_t *root = (uint32_t *)root_pa;
    root[0] = MAKE_PTE(PA_TO_PPN(kern_l0_pa), PTE_V);
    root[1023] = MAKE_PTE(PA_TO_PPN(mmio_l0_pa), PTE_V);
}

void build_user_mappings(uint32_t root_pa, int slot)
{
    uint32_t pa = PROC_SLOT_BASE(slot);
    /* Map physical slot to USER_VA_BASE (0x400000) with U bit */
    map_range(root_pa, USER_VA_BASE, pa, PROC_SLOT_SIZE, PTE_USER_RWX);
    /* Note: shared memory is mapped on demand by sys_shmat */
}

/* ============================
 * Process VM Setup
 * ============================ */

int setup_process_vm(int slot)
{
    struct process *p = &proc_table[slot];
    uint32_t pt_root = pt_alloc();
    if (!pt_root) return -1;

    p->pt_root_pa = pt_root;
    build_kernel_mappings(pt_root);
    build_user_mappings(pt_root, slot);
    /* SATP: SV32 mode, ASID=slot, root PPN */
    p->satp = MAKE_SATP(1, slot, PA_TO_PPN(pt_root));
    return 0;
}

/* ============================
 * Address Translation
 * ============================ */

uint32_t uva_to_pa(int slot, uint32_t uva)
{
    /* User code/data region */
    if (uva >= USER_VA_BASE && uva < USER_VA_END) {
        return PROC_SLOT_BASE(slot) + (uva - USER_VA_BASE);
    }
    /* Shared memory region */
    if (uva >= SHM_VA_BASE && uva < SHM_VA_BASE + MAX_SHM * SHM_SEG_SIZE) {
        return SHM_BASE_ADDR + (uva - SHM_VA_BASE);
    }
    return 0;  /* Invalid address */
}

int copyin(int slot, void *kbuf, uint32_t uva, uint32_t len)
{
    uint32_t pa = uva_to_pa(slot, uva);
    if (!pa) return -1;
    /* Check that entire range is valid */
    uint32_t pa_end = uva_to_pa(slot, uva + len - 1);
    if (!pa_end) return -1;
    memcpy(kbuf, (void *)pa, len);
    return (int)len;
}

int copyout(int slot, uint32_t uva, const void *kbuf, uint32_t len)
{
    uint32_t pa = uva_to_pa(slot, uva);
    if (!pa) return -1;
    uint32_t pa_end = uva_to_pa(slot, uva + len - 1);
    if (!pa_end) return -1;
    memcpy((void *)pa, kbuf, len);
    return (int)len;
}

int copyinstr(int slot, char *kbuf, uint32_t uva, uint32_t maxlen)
{
    uint32_t pa = uva_to_pa(slot, uva);
    if (!pa) return -1;
    const char *src = (const char *)pa;
    uint32_t i;
    for (i = 0; i < maxlen - 1; i++) {
        kbuf[i] = src[i];
        if (src[i] == '\0')
            return (int)i;
    }
    kbuf[i] = '\0';
    return (int)i;
}
