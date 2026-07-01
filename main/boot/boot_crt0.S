# Bootloader entry point for Logisim RISC-V Computer
# Licensed under Creative Commons Attribution International License 4.0
#
# This code resides in ROM at address 0x00000000.
# It initialises the stack, zeros BSS, and calls the C bootloader main.

	.section .init

	.extern __stack_init
	.extern __bss_start
	.extern __bss_end

	.globl _start
_start:
	la sp, __stack_init       # Set stack pointer to top of RAM

	# Zero bootloader BSS section (in high RAM at 0x1F0000)
	la t0, __bss_start
	la t1, __bss_end
1:	bge t0, t1, 2f
	sw zero, 0(t0)
	addi t0, t0, 4
	j 1b
2:
	li a0, 0
	li a1, 0
	li a2, 0
	jal boot_main             # Call C bootloader main

hang:
	j hang                    # Should never reach here
