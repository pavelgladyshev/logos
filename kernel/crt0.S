# Kernel C runtime startup (crt0.S)
# Licensed under Creative Commons Attribution International License 4.0
#
# Initializes the stack pointer, zeroes BSS, and calls main().

	.section .init            # linker configuration file places .init section at
                              # the very start before .text

    .extern __stack_init      # address of the initial top of C call stack (calculated externally
                              # by linker)
    .extern __bss_start
    .extern __bss_end

	.globl _start
_start:                       # this is where CPU starts executing instructions after boot
	la sp,__stack_init        # initialise sp (with the value that points to the last word of RAM)

	# Zero the BSS section
	la t0, __bss_start
	la t1, __bss_end
1:	bge t0, t1, 2f
	sw zero, 0(t0)
	addi t0, t0, 4
	j 1b
2:
	li a0,0                   # populate optional main() parameters with dummy values (just in case)
	li a1,0
	li a2,0
	jal main                  # call C main() function
exit:
	j exit                    # keep looping forever after main() returns
