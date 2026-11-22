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

  /* The kernel has the 64KB below the first process slot (PROC_MEM_START in process.h) */
  ASSERT(_end <= 0x00110000, "kernel too large: it must end below the first process slot at 0x00110000")

  PROVIDE(__stack_init = 0x001FFFFC);
}
