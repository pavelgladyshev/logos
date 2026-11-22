/*
 * Linker script for relocatable (PIE) user programs
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Programs are linked at address 0 and moved to their slot at load time;
 * nothing in them is relocated.
 */

OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /* Start at address 0 - the loader moves the program to its slot */
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
