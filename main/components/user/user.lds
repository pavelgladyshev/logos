/*
 * ESP32-C3 userspace linker layout.
 *
 * Every process receives one 64 KiB physical SRAM slot. The ELF describes a
 * reusable layout within that slot:
 *
 *   physical offset 0x0000-0x7fff  executable code
 *   physical offset 0x8000-0xdfff  constants, data, and BSS
 *   physical offset 0xe000-0xffff  process stack (not stored in the ELF)
 *
 * ESP32-C3 exposes the same SRAM through DRAM and IRAM aliases separated by
 * 0x00700000. Code is therefore linked at virtual address 0x00700000 while
 * byte-addressable data is linked beginning at virtual address 0x00008000.
 * The kernel loader supplies the process-specific DRAM and IRAM base addresses.
 */

OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    USER_IRAM (rx) : ORIGIN = 0x00700000, LENGTH = 0x00008000
    USER_DRAM (rw) : ORIGIN = 0x00008000, LENGTH = 0x00006000
}

/* Produce separate ELF program segments for code and data. */
PHDRS
{
    text PT_LOAD FLAGS(5); /* PF_R | PF_X */
    data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
    /*
     * The code's virtual address is in USER_IRAM. AT(0) records that it is
     * stored at offset zero inside the process's physical SRAM slot.
     */
    .text : AT(0)
    {
        *(.init)
        *(.text)
        *(.text.*)
    } > USER_IRAM :text

    /* Constants use DRAM because strings and tables need byte access. */
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
        *(.srodata)
        *(.srodata.*)
    } > USER_DRAM :data

    .data :
    {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
    } > USER_DRAM :data

    .dynamic :
    {
        *(.dynamic)
    } > USER_DRAM :data

    .got :
    {
        *(.got)
        *(.got.plt)
    } > USER_DRAM :data

    .rela.dyn :
    {
        *(.rela.dyn)
        *(.rela.plt)
    } > USER_DRAM :data

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        __bss_end = .;
    } > USER_DRAM :data

    _end = .;

    ASSERT(SIZEOF(.text) <= LENGTH(USER_IRAM),
           "user program code exceeds its 32 KiB code area")

    ASSERT(_end <= ORIGIN(USER_DRAM) + LENGTH(USER_DRAM),
           "user program data leaves less than 8 KiB for its stack")
}
