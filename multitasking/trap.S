	.text       

# function set_trap_handler(trap_handler(), trap_frame_t*) stores the address 
# of assembly language trap handling routine in mtvec register and
# the address of the trap_frame data sturcture in mscratch register
        .globl set_trap_handler
set_trap_handler:
        csrw mtvec,a0
        csrw mscratch,a1
        ret

# functions set_mie(val) and get_mie() setting / returning value 
# of Machine Interrupts Enable register (mie)
        .globl set_mie
set_mie:
        csrw mie,a0
        ret

        .globl get_mie
get_mie:
        csrr a0,mie
        ret

        .globl set_mstatus_bit
set_mstatus_bit:
 	csrs mstatus,a0    # set machine previous interrupt enable bit in mstatus register
        ret

# functions to do global enabling and disabling of interrupt and exception handling
        .globl enable_interrupts
enable_interrupts:
 	csrsi mstatus,8    # set machine interrupt enable bit in mstatus register
        ret

        .globl disable_interrupts
disable_interrupts:
        csrci mstatus,8    # clear machine interrupt enable bit in mstatus register
        ret


# trap_handler - entry point for interrupts and exceptions
	.globl trap_handler
trap_handler:
	# swap a0 and mscratch
        # so that a0 points to the trap_frame structure
        csrrw a0, mscratch, a0

        # save the main program's execution context into the trap_frame structure
        sw ra, 20(a0)
        sw sp, 24(a0)
        sw gp, 28(a0)
        sw tp, 32(a0)
        sw t0, 36(a0)
        sw t1, 40(a0)
        sw t2, 44(a0)
        sw s0, 48(a0)
        sw s1, 52(a0)
        sw a1, 60(a0)
        sw a2, 64(a0)
        sw a3, 68(a0)
        sw a4, 72(a0)
        sw a5, 76(a0)
        sw a6, 80(a0)
        sw a7, 84(a0)
        sw s2, 88(a0)
        sw s3, 92(a0)
        sw s4, 96(a0)
        sw s5, 100(a0)
        sw s6, 104(a0)
        sw s7, 108(a0)
        sw s8, 112(a0)
        sw s9, 116(a0)
        sw s10, 120(a0)
        sw s11, 124(a0)
        sw t3, 128(a0)
        sw t4, 132(a0)
        sw t5, 136(a0)
        sw t6, 140(a0)

        # save mepc in trap_frame->mepc
        csrr t0, mepc
        sw t0, 12(a0)

	# save the user a0 in trap_frame->a0
        csrr t0, mscratch
        sw t0, 56(a0)

        # load stack pointer value from trap_frame->sp
        lw sp, 4(a0)

        # load the address of C language trap handler, c_trap(), from trap_frame->c_trap
        lw t0, 8(a0)

        # jump to c_trap(frap_frame_t *trap_frame)  giving trap_frame pointer as the parameter
        jr t0



# int get_mcause() - return value of mcause that indicates reason for the trap
        .globl get_mcause
get_mcause:
        csrr a0,mcause
        ret



# void trap_ret(trap_frame_t *trap_frame)
        .globl trap_ret
trap_ret:
        # restore mepc from trap_frame->mepc
        lw t0, 12(a0)
        csrw mepc, t0

        # put the saved  trap_frame->a0 in mscratch, so we
        # can swap it with a0 in the last step.
        lw t0, 56(a0)
        csrw mscratch, t0

        # restore all but a0 from trap_frame
        lw ra, 20(a0)
        lw sp, 24(a0)
        lw gp, 28(a0)
        lw tp, 32(a0)
        lw t0, 36(a0)
        lw t1, 40(a0)
        lw t2, 44(a0)
        lw s0, 48(a0)
        lw s1, 52(a0)
        lw a1, 60(a0)
        lw a2, 64(a0)
        lw a3, 68(a0)
        lw a4, 72(a0)
        lw a5, 76(a0)
        lw a6, 80(a0)
        lw a7, 84(a0)
        lw s2, 88(a0)
        lw s3, 92(a0)
        lw s4, 96(a0)
        lw s5, 100(a0)
        lw s6, 104(a0)
        lw s7, 108(a0)
        lw s8, 112(a0)
        lw s9, 116(a0)
        lw s10, 120(a0)
        lw s11, 124(a0)
        lw t3, 128(a0)
        lw t4, 132(a0)
        lw t5, 136(a0)
        lw t6, 140(a0)
         //process input from inp_buff[]
	# restore user a0, and save trap_frame pointer in mscratch
        csrrw a0, mscratch, a0
        
        # return to the interrupted program (pc address is in mepc)
        mret

