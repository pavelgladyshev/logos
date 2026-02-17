OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY 
{
  rom (rxi)  : ORIGIN = 0x00000000, LENGTH = 64K
  ram (w!i)  : ORIGIN = 0x00010000, LENGTH = 64K
}

SECTIONS
{
  .init : { *(.init) } > rom
  .text : { *(.text) } > rom
  .srodata : { *(.srodata) } > rom
  .data : { *(.data) } > rom

  .bss : { *(.bss) *(COMMON) } > ram
  
  PROVIDE(__stack_init = ORIGIN(ram) + LENGTH(ram) - 4);
}

