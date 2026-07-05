/*
 * ELF Loader for RISC-V
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Loads ELF executables from the filesystem into RAM and executes them.
 * Supports both fixed-address (ET_EXEC) and relocatable (ET_DYN/PIE) executables.
 */

#ifndef LOADER_H
#define LOADER_H

#include "types.h"

/* Loader error codes */
#define LOAD_OK             0
#define LOAD_ERR_NOT_FOUND  (-1)  /* File not found */
#define LOAD_ERR_IO         (-2)  /* I/O error reading file */
#define LOAD_ERR_FORMAT     (-3)  /* Invalid ELF format */
#define LOAD_ERR_ARCH       (-4)  /* Wrong architecture (not RISC-V 32-bit) */
#define LOAD_ERR_TOO_LARGE  (-5)  /* Program too large for memory */
#define LOAD_ERR_BAD_ADDR   (-6)  /* Segment address out of valid range */
#define LOAD_ERR_RELOC      (-7)  /* Relocation error */

/* Memory layout for loaded programs */
#define PROGRAM_LOAD_ADDR   0x00110000  /* Default base address for programs */
#define PROGRAM_MAX_SIZE    0x000E0000  /* ~896KB max program size */
#define PROGRAM_STACK_TOP   0x001FFF00  /* Initial stack pointer for programs */

/* Program information returned by loader */
struct program_info {
    uint32_t entry_point;   /* Address to jump to */
    uint32_t load_base;     /* Lowest loaded address */
    uint32_t load_end;      /* Highest loaded address + 1 */
};

/*
 * Load an ELF executable from the filesystem into memory.
 * Uses PROGRAM_LOAD_ADDR as the base address.
 *
 * path: Absolute path to executable (e.g., "/bin/hello")
 * info: Output structure filled with program information
 *
 * Returns: LOAD_OK on success, negative error code on failure
 */
int elf_load(const char *path, struct program_info *info);

/*
 * Load an ELF executable at a specified base address.
 * For relocatable (PIE) executables, relocations are applied.
 *
 * path: Absolute path to executable (e.g., "/bin/hello")
 * load_addr: Base address to load the program at
 * info: Output structure filled with program information
 *
 * Returns: LOAD_OK on success, negative error code on failure
 */
int elf_load_at(const char *path, uint32_t load_addr, uint32_t max_size, struct program_info *info);

/*
 * Execute a loaded program.
 *
 * info: Program information from elf_load()
 *
 * Returns: Return value from program's main()
 */
int elf_exec(struct program_info *info);

#endif /* LOADER_H */
