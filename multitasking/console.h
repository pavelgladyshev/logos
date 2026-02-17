/*
 * Example software running in Logisim RISC-V Computer System model by Pavel Gladyshev
 * licensed under Creative Commons Attribution International license 4.0
 *
 * This file defines addresses for memory mapped keyboard and TTY display
 */

#pragma once

#define TTY ((volatile unsigned int*) 0xFFFF0000)
#define RCR ((volatile unsigned int*) 0xFFFF0004)
#define RDR ((volatile unsigned int*) 0xFFFF0008)

// Monitor request button. Generates external interrupt when pressed
// Reading it show interrupt request state
// Writing anything into it clears interrupt request
#define MONITOR_REQ_BUTTON ((volatile unsigned int *)0xFFFF0040)
