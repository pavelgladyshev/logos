/* Logisim console device constants and TTY MMIO registers. */

#ifndef LOGISIM_CONSOLE_CONSTANTS_H
#define LOGISIM_CONSOLE_CONSTANTS_H

#include "types.h"

#define CONSOLE_MAJOR 1
#define CONSOLE_MINOR 0

#define CONSOLE_DATA (*(volatile uint32_t *)0xFFFF000C)
#define CONSOLE_RCR  (*(volatile uint32_t *)0xFFFF0004)
#define CONSOLE_RDR  (*(volatile uint32_t *)0xFFFF0008)

#endif /* LOGISIM_CONSOLE_CONSTANTS_H */
