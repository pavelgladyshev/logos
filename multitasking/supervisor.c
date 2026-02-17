/*
 * Example software running in Logisim RISC-V Computer System model by Pavel Gladyshev
 * licensed under Creative Commons Attribution International license 4.0
 *
 * This example shows how graphics display can be used to dsisplay a dot bouncing
 * off the screen borders.
 *
 * the position of the ball can be changed by typing new x,y coordinates on the keyboard
 * and pressing Enter key. Coordinate values are in 0..63 range. The origin (0,0) is in
 * the top left corner of the vga display
 *
 */

#include "console.h"
#include "timer.h"
#include "trap.h"
#include "process.h"


// maximal number of processes
#define N_PROC (3)
process_t processes[N_PROC+1];

// currently running process (UNINITIALISED!)
int current_pid;

// portion of time alotted to each task
#define TIME_SLICE (800)

// program stacks for the C language trap handler code
int trap_handler_stack[256];

void c_trap_handler(trap_frame_t *tf);

void init_interrupts()
{
   // enable interrupts from external sources and timer by setting bits 11 and 7 in CPU's mie register
   set_mie(0x880);

   // set timer alarm 800 system clocks ahead of mtime
   *TIMER_MTIMECMP = *TIMER_MTIME + TIME_SLICE; 

   // enable handling of interrupts upon mret in CPU by setting bit MPIE bit in CPU's mstatus register
   set_mstatus_bit(0x80);
}

void init_processes() 
{
   int i;

   for (i = 0; i < N_PROC; i++)
   {
      processes[i].pid = i;
      processes[i].state = UNUSED;
      processes[i].tf.c_trap = (uint32)c_trap_handler;
      processes[i].tf.c_trap_sp = (uint32)trap_handler_stack;
   }

   current_pid = -1;

}

int start_process(uint32 start_addr) 
{
   int i;

   for (i=0; i< N_PROC; i++) 
   {
      if (processes[i].state == UNUSED) 
      {
         processes[i].tf.mepc = start_addr;
         processes[i].state = RUNNING;
         return processes[i].pid;
      }
   }
   return -1;  // no available process structures
}

void run_next_process()
{
   for(;;) {
      current_pid++;
      if (current_pid >= N_PROC)
      {
         current_pid = 0;
      }
      if (processes[current_pid].state == RUNNING) 
      {
         set_trap_handler(trap_handler, &(processes[current_pid].tf));
         trap_ret(&(processes[current_pid].tf)); // never returns
      }
   }
}

/* print string to TTY */
void printstr(char *str)
{
   while (*str)
      *TTY = *str++;
}

void main()
{
   init_processes();
   start_process(0x20000); // start program in User 1 RAM
   start_process(0x30000); // start program in User 2 RAM
   start_process(0x40000); // start program in User 2 RAM
   init_interrupts();
   run_next_process(); //never returns
}

/*
 * Trap handler written in C language (jumped to from
 * the assembly language trap_handler)
 */
void c_trap_handler(trap_frame_t *tf)
{

   uint32 cause;
   int key;

   cause = get_mcause();

   switch (cause)
   {

      case MCAUSE_TIMER | MCAUSE_INTERRUPT:
      {
         // advance the alarm
         *TIMER_MTIMECMP = *TIMER_MTIME + TIME_SLICE; 
         break;
      }

      case MCAUSE_EXTERNAL | MCAUSE_INTERRUPT:
      {
         // handle hardware interrupts 
         // panic for now :)
         printstr("Panic: unexpected hardware interrupt!\n");
         break;
      }

      default:  
      {
         // unexpected interruption cause: panic :)
         printstr("Panic: unexpected exception or interrupt!\n");
      }

   }

   run_next_process(); // never returns
}
