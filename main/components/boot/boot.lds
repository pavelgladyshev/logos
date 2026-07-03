OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    iram (rxi) : ORIGIN = 0x40380000, LENGTH = 64K
    dram (rw) : ORIGIN = 0x3FC80000, LENGTH = 64K
}

SECTIONS
{
  .init : { *(.init) } > iram 
  .text : { *(.text) *(.text.*) } > iram
  .rodata : { *(.rodata) *(.rodata.*) } > iram
  .srodata : { *(.srodata) *(.srodata.*) } > iram
  .data : { *(.data) *(.data.*) } > dram
  .sdata : { *(.sdata) *(.sdata.*) } > dram

  .bss : {
    __bss_start = .;
    *(.bss) *(.sbss) *(COMMON)
    __bss_end = .;
  } > dram

  PROVIDE(__stack_init = 00x3FC9000);
}
