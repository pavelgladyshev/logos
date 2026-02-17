OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY 
{
  user_ram (rwxi)  : ORIGIN = 0x00030000, LENGTH = 64K
}

SECTIONS
{
  .init : { *(.init) } > user_ram
  .text : { *(.text) } > user_ram
  .srodata : { *(.srodata) } > user_ram
  .data : { *(.data) } > user_ram
  .bss : { *(.bss) *(COMMON) } > user_ram
  
  PROVIDE(__stack_init = ORIGIN(user_ram) + LENGTH(user_ram) - 4);
}

