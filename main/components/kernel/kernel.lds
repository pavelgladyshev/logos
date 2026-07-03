OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  . = 0x00100000;

  .init : { *(.init) }
  .text : { *(.text) *(.text.*) }
  .rodata : { *(.rodata) *(.rodata.*) }
  .srodata : { *(.srodata) *(.srodata.*) }
  .data : { *(.data) *(.data.*) }
  .sdata : { *(.sdata) *(.sdata.*) }

  .bss : {
    __bss_start = .;
    *(.bss) *(.sbss) *(COMMON)
    __bss_end = .;
  }

  _end = .;

  PROVIDE(__stack_init = 0x001FFFFC);
}
