/*
 * Linker script for relocatable (PIE) user programs
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Programs are linked at address 0 and relocated at load time.
 */

OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /* Start at address 0 - will be relocated at load time */
    . = 0;

    .text : {
        *(.init)
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.srodata)
        *(.srodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
    }

    /* Dynamic section for relocation info */
    .dynamic : {
        *(.dynamic)
    }

    /* Global Offset Table */
    .got : {
        *(.got)
        *(.got.plt)
    }

    /* Relocation tables */
    .rela.dyn : {
        *(.rela.dyn)
        *(.rela.plt)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        __bss_end = .;
    }

    _end = .;
}
