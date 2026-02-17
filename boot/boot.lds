OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
  rom (rxi)      : ORIGIN = 0x00000000, LENGTH = 64K
  boot_ram (w)   : ORIGIN = 0x001F0000, LENGTH = 60K
}

SECTIONS
{
  .init : { *(.init) } > rom
  .text : { *(.text) *(.text.*) } > rom
  .rodata : { *(.rodata) *(.rodata.*) } > rom
  .srodata : { *(.srodata) *(.srodata.*) } > rom
  .data : { *(.data) *(.data.*) } > rom
  .sdata : { *(.sdata) *(.sdata.*) } > rom

  .bss : {
    __bss_start = .;
    *(.bss) *(.sbss) *(COMMON)
    __bss_end = .;
  } > boot_ram

  PROVIDE(__stack_init = 0x001FFFFC);
}
