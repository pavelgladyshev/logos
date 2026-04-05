/*
 * Timer constants shared between kernel modules and M-mode assembly.
 */
#ifndef TIMER_H
#define TIMER_H

/* CLINT timer registers (memory-mapped) */
#define TIMER_MTIME_ADDR     0x200bff8
#define TIMER_MTIMECMP_ADDR  0x2004000

/* Timer tick interval (in timer cycles) */
#define TIME_SLICE           800

/* C-friendly volatile pointers (not usable from assembly) */
#ifndef __ASSEMBLER__
#define TIMER_MTIME    ((volatile uint32_t *)TIMER_MTIME_ADDR)
#define TIMER_MTIMECMP ((volatile uint32_t *)TIMER_MTIMECMP_ADDR)
#endif

#endif /* TIMER_H */
