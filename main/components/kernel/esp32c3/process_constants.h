
/* Process table limits */
#ifndef PROCESS_CONSTANTS_H
#define PROCESS_CONSTANTS_H

#define MAX_PROCS       2
#define PROC_SLOT_SIZE  0x10000

extern unsigned char proc_memory[MAX_PROCS * PROC_SLOT_SIZE]; 

/* PROC_MEM_START is the writable DRAM view of process memory. */
#define PROC_MEM_START ((uint32_t)proc_memory)
#define PROC_IRAM_START (PROC_MEM_START + 0x00700000)



/* Original name: the base stored in struct process is the DRAM view. */
#define PROC_SLOT_BASE(n) \
  (PROC_MEM_START + (n) * PROC_SLOT_SIZE)

/* Executable alias of the same physical process slot. */
#define PROC_SLOT_IRAM_BASE(n) \
  (PROC_IRAM_START + (n) * PROC_SLOT_SIZE)

#define PROC_SLOT_STACK(n) \
  (PROC_SLOT_BASE(n) + PROC_SLOT_SIZE - 0x100)

#endif
