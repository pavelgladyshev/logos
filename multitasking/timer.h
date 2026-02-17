/*
 * Example software running in Logisim RISC-V Computer System model by Pavel Gladyshev
 * licensed under Creative Commons Attribution International license 4.0
 *
 * This file contains constants for memory mapped timer registers (Timer)
 */

#pragma once

// mtime: 31-bit memory mapped counter counting system clock ticks, which silently wraps around. 
// Readable and writable. Initialised to 0 on reset.
#define TIMER_MTIME ((unsigned int *)(0x200bff8))

// mtimecmp: 32-bit memory mapped register storing the value of the "alarm" - timer emits 
// Timer Interrupt Request == 1 whenever the value of mtime >= mtimecmp. Readable and writable.
// Initialised to 0xffffffff (-1) so that it does not generate interrupt requests by default.
#define TIMER_MTIMECMP ((unsigned int *)(0x2004000))