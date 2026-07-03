#pragma once

#include <stdint.h>

/* Busy-wait delay used before logOS has a scheduler or timer interrupt path. */
void kernel_delay_ms(uint32_t ms);
