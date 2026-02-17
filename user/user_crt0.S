# C runtime startup for user programs
# Licensed under Creative Commons Attribution International License 4.0
#
# Entry point for programs loaded by the kernel.
# The kernel sets up the stack and trap frame, then jumps here via mret.
# Programs must use the exit syscall to terminate.
#
# Kernel passes:
#   a0 = argc (argument count)
#   a1 = argv (pointer to argument array)

    .section .init
    .globl _start
    .type _start, @function

_start:
    # Stack is already set up by the kernel
    # a0 = argc, a1 = argv are already set by kernel

    # Call main(argc, argv)
    jal  main

    # main() returned - call exit syscall with return value
    # a0 already contains main's return value
    li   a7, 0      # SYS_exit = 0
    ecall

    # Should never reach here, but infinite loop just in case
1:  j    1b

    .size _start, .-_start
