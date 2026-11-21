/*
 * The machine timer: mtime and mtimecmp
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * mtime counts clock ticks, and the timer requests an interrupt while
 * mtime >= mtimecmp. Both registers have 64 bits, on a 32-bit RISC-V too
 * (the RISC-V privileged specification, "Machine Timer Registers"): with
 * 32 bits and a tick of a microsecond, mtime would be back at 0 after 72
 * minutes. Each register is two words in memory, the low word first.
 *
 * The specification does not say what mtimecmp holds after reset, so the
 * kernel sets it, both words, before it enables the timer interrupt.
 */
#ifndef TIMER_H
#define TIMER_H

/* CLINT timer registers (memory-mapped): the low word; the high word is 4 above */
#define TIMER_MTIME_ADDR     0x200bff8
#define TIMER_MTIMECMP_ADDR  0x2004000

/* Timer tick interval (in timer cycles) */
#define TIME_SLICE           800

/* The rest is C: an assembly file takes only the constants above */
#ifndef __ASSEMBLER__
#include "types.h"

/* The two words of each register: [0] is the low word, [1] the high word */
#define TIMER_MTIME    ((volatile uint32_t *)TIMER_MTIME_ADDR)
#define TIMER_MTIMECMP ((volatile uint32_t *)TIMER_MTIMECMP_ADDR)

/*
 * Read mtime, all 64 bits. It counts on while its two words are read one
 * after the other, and the low word may carry into the high word in
 * between: so read the high word, the low word and the high word again,
 * and repeat if the high word has changed.
 */
static inline uint64_t timer_read_mtime(void)
{
    uint32_t high, low;

    do {
        high = TIMER_MTIME[1];
        low = TIMER_MTIME[0];
    } while (TIMER_MTIME[1] != high);
    return ((uint64_t)high << 32) | low;
}

/*
 * Set mtimecmp, all 64 bits, with the three stores that the specification
 * gives for a 32-bit RISC-V. A store changes one word only, and with two
 * stores mtimecmp could for a moment be below both its old and its new
 * value: the timer would request an interrupt that neither asks for.
 * Storing all ones into the low word first prevents that.
 */
static inline void timer_write_mtimecmp(uint64_t value)
{
    TIMER_MTIMECMP[0] = 0xffffffff;               /* not below the old value */
    TIMER_MTIMECMP[1] = (uint32_t)(value >> 32);  /* not below the new value */
    TIMER_MTIMECMP[0] = (uint32_t)value;          /* the new value */
}

/* Ask for the next timer interrupt, TIME_SLICE ticks from now */
static inline void timer_arm(void)
{
    timer_write_mtimecmp(timer_read_mtime() + TIME_SLICE);
}
#endif /* __ASSEMBLER__ */

#endif /* TIMER_H */
