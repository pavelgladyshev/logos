OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY 
{
  rom (rxi)  : ORIGIN = 0x00000000, LENGTH = 64K
  ram (w!i)  : ORIGIN = 0x00100000, LENGTH = 1M 
}

SECTIONS
{
  .init : { *(.init) } > rom
  .text : { *(.text) *(.text.*) } > rom
  .rodata : { *(.rodata) *(.rodata.*) } > rom
  .srodata : { *(.srodata) *(.srodata.*) } > rom
  .data : { *(.data) *(.data.*) } > rom
  .sdata : { *(.sdata) *(.sdata.*) } > rom

  .bss : { *(.bss) *(.sbss) *(COMMON) } > ram

  PROVIDE(__stack_init = ORIGIN(ram) + LENGTH(ram) - 4);
}

