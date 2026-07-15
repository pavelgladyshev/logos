/*
 * ELF format definitions for RISC-V 32-bit
 * Licensed under Creative Commons Attribution International License 4.0
 */

#ifndef ELF_H
#define ELF_H

#include "types.h"

/* ELF Magic Number */
#define ELF_MAGIC_0     0x7F
#define ELF_MAGIC_1     'E'
#define ELF_MAGIC_2     'L'
#define ELF_MAGIC_3     'F'

/* ELF Class (32-bit vs 64-bit) */
#define ELFCLASS32      1
#define ELFCLASS64      2

/* ELF Data Encoding */
#define ELFDATA2LSB     1   /* Little-endian */
#define ELFDATA2MSB     2   /* Big-endian */

/* ELF Machine Types */
#define EM_RISCV        243 /* RISC-V */

/* ELF Types */
#define ET_NONE         0   /* No file type */
#define ET_REL          1   /* Relocatable file */
#define ET_EXEC         2   /* Executable file */
#define ET_DYN          3   /* Shared object file */

/* Program Header Types */
#define PT_NULL         0   /* Unused entry */
#define PT_LOAD         1   /* Loadable segment */
#define PT_DYNAMIC      2   /* Dynamic linking info */
#define PT_INTERP       3   /* Interpreter path */
#define PT_NOTE         4   /* Auxiliary information */

/* Program Header Flags */
#define PF_X            0x1 /* Execute permission */
#define PF_W            0x2 /* Write permission */
#define PF_R            0x4 /* Read permission */

/* e_ident[] array indices */
#define EI_MAG0         0   /* File identification byte 0 */
#define EI_MAG1         1   /* File identification byte 1 */
#define EI_MAG2         2   /* File identification byte 2 */
#define EI_MAG3         3   /* File identification byte 3 */
#define EI_CLASS        4   /* File class */
#define EI_DATA         5   /* Data encoding */
#define EI_VERSION      6   /* File version */
#define EI_OSABI        7   /* OS/ABI identification */
#define EI_ABIVERSION   8   /* ABI version */
#define EI_NIDENT       16  /* Size of e_ident[] */

/* ELF32 Header Structure */
typedef struct {
    uint8_t  e_ident[EI_NIDENT]; /* ELF identification */
    uint16_t e_type;             /* Object file type */
    uint16_t e_machine;          /* Machine type */
    uint32_t e_version;          /* Object file version */
    uint32_t e_entry;            /* Entry point address */
    uint32_t e_phoff;            /* Program header offset */
    uint32_t e_shoff;            /* Section header offset */
    uint32_t e_flags;            /* Processor-specific flags */
    uint16_t e_ehsize;           /* ELF header size */
    uint16_t e_phentsize;        /* Program header entry size */
    uint16_t e_phnum;            /* Number of program headers */
    uint16_t e_shentsize;        /* Section header entry size */
    uint16_t e_shnum;            /* Number of section headers */
    uint16_t e_shstrndx;         /* Section name string table index */
} Elf32_Ehdr;

/* ELF32 Program Header Structure */
typedef struct {
    uint32_t p_type;    /* Segment type */
    uint32_t p_offset;  /* Offset in file */
    uint32_t p_vaddr;   /* Virtual address in memory */
    uint32_t p_paddr;   /* Physical address (usually same as vaddr) */
    uint32_t p_filesz;  /* Size in file */
    uint32_t p_memsz;   /* Size in memory (may be larger for BSS) */
    uint32_t p_flags;   /* Segment flags (PF_R, PF_W, PF_X) */
    uint32_t p_align;   /* Alignment */
} Elf32_Phdr;

/* ELF32 Dynamic Section Entry */
typedef struct {
    int32_t  d_tag;     /* Dynamic entry type */
    uint32_t d_val;     /* Integer value or address */
} Elf32_Dyn;

/* Dynamic section tags */
#define DT_NULL         0   /* End of dynamic section */
#define DT_RELA         7   /* Address of Rela relocation table */
#define DT_RELASZ       8   /* Size of Rela relocation table (bytes) */
#define DT_RELAENT      9   /* Size of one Rela relocation entry */
#define DT_REL          17  /* Address of Rel relocation table */
#define DT_RELSZ        18  /* Size of Rel relocation table (bytes) */
#define DT_RELENT       19  /* Size of one Rel relocation entry */
#define DT_RELACOUNT    0x6ffffff9  /* Count of RELATIVE relocations */

/* ELF32 Relocation Entry with Addend (Rela) */
typedef struct {
    uint32_t r_offset;  /* Location to apply relocation */
    uint32_t r_info;    /* Relocation type and symbol index */
    int32_t  r_addend;  /* Addend value */
} Elf32_Rela;

/* ELF32 Relocation Entry without Addend (Rel) */
typedef struct {
    uint32_t r_offset;  /* Location to apply relocation */
    uint32_t r_info;    /* Relocation type and symbol index */
} Elf32_Rel;

/* Relocation info macros */
#define ELF32_R_SYM(info)   ((info) >> 8)
#define ELF32_R_TYPE(info)  ((info) & 0xff)

/* RISC-V Relocation Types */
#define R_RISCV_NONE        0
#define R_RISCV_32          1   /* 32-bit absolute */
#define R_RISCV_RELATIVE    3   /* Adjust by load bias */

#endif /* ELF_H */
