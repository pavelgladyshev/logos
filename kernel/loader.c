/*
 * ELF Loader implementation for RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Supports both fixed-address (ET_EXEC) and relocatable (ET_DYN/PIE) executables.
 */

#include "loader.h"
#include "elf.h"
#include "fs.h"
#include "console.h"

/* Assembly trampoline for executing loaded program */
extern int elf_trampoline(uint32_t entry, uint32_t sp);

/*
 * Validate ELF header
 * Returns LOAD_OK if valid, error code otherwise
 */
static int validate_elf_header(Elf32_Ehdr *ehdr)
{
    /* Check magic number: 0x7F 'E' 'L' 'F' */
    if (ehdr->e_ident[EI_MAG0] != ELF_MAGIC_0 ||
        ehdr->e_ident[EI_MAG1] != ELF_MAGIC_1 ||
        ehdr->e_ident[EI_MAG2] != ELF_MAGIC_2 ||
        ehdr->e_ident[EI_MAG3] != ELF_MAGIC_3) {
        return LOAD_ERR_FORMAT;
    }

    /* Check 32-bit ELF */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        return LOAD_ERR_ARCH;
    }

    /* Check little-endian */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return LOAD_ERR_ARCH;
    }

    /* Check machine type is RISC-V */
    if (ehdr->e_machine != EM_RISCV) {
        return LOAD_ERR_ARCH;
    }

    /* Check executable type (ET_EXEC or ET_DYN for PIE) */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return LOAD_ERR_FORMAT;
    }

    /* Sanity check program header info */
    if (ehdr->e_phnum == 0 || ehdr->e_phentsize < sizeof(Elf32_Phdr)) {
        return LOAD_ERR_FORMAT;
    }

    return LOAD_OK;
}

/*
 * Apply relocations to a loaded PIE executable
 * load_bias: difference between actual load address and linked address
 */
static int apply_relocations(uint32_t ino, Elf32_Ehdr *ehdr, uint32_t load_bias)
{
    Elf32_Phdr phdr;
    uint32_t ph_offset;
    uint32_t dyn_offset = 0;
    uint32_t dyn_size = 0;
    int result;
    uint16_t i;

    /* Find PT_DYNAMIC segment */
    ph_offset = ehdr->e_phoff;
    for (i = 0; i < ehdr->e_phnum; i++) {
        result = file_read(ino, ph_offset, &phdr, sizeof(Elf32_Phdr));
        if (result != sizeof(Elf32_Phdr)) {
            return LOAD_ERR_IO;
        }
        ph_offset += ehdr->e_phentsize;

        if (phdr.p_type == PT_DYNAMIC) {
            dyn_offset = phdr.p_offset;
            dyn_size = phdr.p_filesz;
            break;
        }
    }

    /* No dynamic section means no relocations needed */
    if (dyn_offset == 0) {
        return LOAD_OK;
    }

    /* Parse dynamic section to find relocation table */
    uint32_t rela_addr = 0;
    uint32_t rela_size = 0;
    uint32_t rela_ent = 0;
    uint32_t rel_addr = 0;
    uint32_t rel_size = 0;
    uint32_t rel_ent = 0;

    uint32_t dyn_pos = dyn_offset;
    while (dyn_pos < dyn_offset + dyn_size) {
        Elf32_Dyn dyn;
        result = file_read(ino, dyn_pos, &dyn, sizeof(Elf32_Dyn));
        if (result != sizeof(Elf32_Dyn)) {
            return LOAD_ERR_IO;
        }
        dyn_pos += sizeof(Elf32_Dyn);

        if (dyn.d_tag == DT_NULL) {
            break;
        }

        switch (dyn.d_tag) {
            case DT_RELA:
                rela_addr = dyn.d_val;
                break;
            case DT_RELASZ:
                rela_size = dyn.d_val;
                break;
            case DT_RELAENT:
                rela_ent = dyn.d_val;
                break;
            case DT_REL:
                rel_addr = dyn.d_val;
                break;
            case DT_RELSZ:
                rel_size = dyn.d_val;
                break;
            case DT_RELENT:
                rel_ent = dyn.d_val;
                break;
        }
    }

    /* Apply RELA relocations (with explicit addend) */
    if (rela_addr != 0 && rela_size != 0 && rela_ent >= sizeof(Elf32_Rela)) {
        uint32_t num_relas = rela_size / rela_ent;
        /* rela_addr is a virtual address, need to read from memory after loading */
        Elf32_Rela *rela_table = (Elf32_Rela *)(rela_addr + load_bias);

        for (i = 0; i < num_relas; i++) {
            Elf32_Rela *rela = &rela_table[i];
            uint32_t type = ELF32_R_TYPE(rela->r_info);

            if (type == R_RISCV_RELATIVE) {
                /* R_RISCV_RELATIVE: *(addr + offset) = load_bias + addend */
                uint32_t *target = (uint32_t *)(rela->r_offset + load_bias);
                *target = load_bias + rela->r_addend;
            }
            /* Other relocation types not supported (would need symbol table) */
        }
    }

    /* Apply REL relocations (addend in memory) */
    if (rel_addr != 0 && rel_size != 0 && rel_ent >= sizeof(Elf32_Rel)) {
        uint32_t num_rels = rel_size / rel_ent;
        Elf32_Rel *rel_table = (Elf32_Rel *)(rel_addr + load_bias);

        for (i = 0; i < num_rels; i++) {
            Elf32_Rel *rel = &rel_table[i];
            uint32_t type = ELF32_R_TYPE(rel->r_info);

            if (type == R_RISCV_RELATIVE) {
                /* R_RISCV_RELATIVE: *addr += load_bias */
                uint32_t *target = (uint32_t *)(rel->r_offset + load_bias);
                *target += load_bias;
            }
        }
    }

    return LOAD_OK;
}

/*
 * Load an ELF executable at a specified base address
 */
int elf_load_at(const char *path, uint32_t load_addr, struct program_info *info)
{
    uint32_t ino;
    int result;
    Elf32_Ehdr ehdr;
    Elf32_Phdr phdr;
    uint32_t ph_offset;
    uint16_t i;
    uint32_t load_bias;
    uint32_t min_vaddr = 0xFFFFFFFF;

    /* Open the executable file */
    result = fs_open(path, &ino);
    if (result != FS_OK) {
        return LOAD_ERR_NOT_FOUND;
    }

    /* Read ELF header */
    result = file_read(ino, 0, &ehdr, sizeof(Elf32_Ehdr));
    if (result != sizeof(Elf32_Ehdr)) {
        return LOAD_ERR_IO;
    }

    /* Validate header */
    result = validate_elf_header(&ehdr);
    if (result != LOAD_OK) {
        return result;
    }

    /* First pass: find minimum virtual address to calculate load bias */
    ph_offset = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        result = file_read(ino, ph_offset, &phdr, sizeof(Elf32_Phdr));
        if (result != sizeof(Elf32_Phdr)) {
            return LOAD_ERR_IO;
        }
        ph_offset += ehdr.e_phentsize;

        if (phdr.p_type == PT_LOAD && phdr.p_memsz > 0) {
            if (phdr.p_vaddr < min_vaddr) {
                min_vaddr = phdr.p_vaddr;
            }
        }
    }

    if (min_vaddr == 0xFFFFFFFF) {
        return LOAD_ERR_FORMAT;
    }

    /* Calculate load bias (difference between requested and linked address) */
    load_bias = load_addr - min_vaddr;

    /* Initialize program info */
    info->entry_point = ehdr.e_entry + load_bias;
    info->load_base = 0xFFFFFFFF;
    info->load_end = 0;

    /* Second pass: load segments */
    ph_offset = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        result = file_read(ino, ph_offset, &phdr, sizeof(Elf32_Phdr));
        if (result != sizeof(Elf32_Phdr)) {
            return LOAD_ERR_IO;
        }
        ph_offset += ehdr.e_phentsize;

        /* Only process loadable segments */
        if (phdr.p_type != PT_LOAD) {
            continue;
        }

        /* Skip empty segments */
        if (phdr.p_memsz == 0) {
            continue;
        }

        /* Calculate actual load address with bias */
        uint32_t seg_addr = phdr.p_vaddr + load_bias;

        /* Validate segment address is in valid RAM range */
        if (seg_addr < load_addr) {
            return LOAD_ERR_BAD_ADDR;
        }
        if (seg_addr + phdr.p_memsz > load_addr + PROGRAM_MAX_SIZE) {
            return LOAD_ERR_TOO_LARGE;
        }

        /* Track load bounds */
        if (seg_addr < info->load_base) {
            info->load_base = seg_addr;
        }
        if (seg_addr + phdr.p_memsz > info->load_end) {
            info->load_end = seg_addr + phdr.p_memsz;
        }

        /* Load segment data from file (DMA direct to destination) */
        if (phdr.p_filesz > 0) {
            result = file_load_direct(ino, phdr.p_offset, (void *)seg_addr, phdr.p_filesz);
            if (result != (int)phdr.p_filesz) {
                return LOAD_ERR_IO;
            }
        }

        /* Zero BSS portion (memsz > filesz) */
        if (phdr.p_memsz > phdr.p_filesz) {
            uint32_t bss_addr = seg_addr + phdr.p_filesz;
            uint32_t bss_size = phdr.p_memsz - phdr.p_filesz;
            memset((void *)bss_addr, 0, bss_size);
        }
    }

    /* Verify we loaded something */
    if (info->load_base == 0xFFFFFFFF) {
        return LOAD_ERR_FORMAT;
    }

    /* Apply relocations for PIE executables */
    if (ehdr.e_type == ET_DYN && load_bias != 0) {
        result = apply_relocations(ino, &ehdr, load_bias);
        if (result != LOAD_OK) {
            return result;
        }
    }

    return LOAD_OK;
}

/*
 * Load an ELF executable at the default address
 */
int elf_load(const char *path, struct program_info *info)
{
    return elf_load_at(path, PROGRAM_LOAD_ADDR, info);
}

/*
 * Execute a loaded program
 */
int elf_exec(struct program_info *info)
{
    return elf_trampoline(info->entry_point, PROGRAM_STACK_TOP);
}
