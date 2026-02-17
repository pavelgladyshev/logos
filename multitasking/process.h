/*
 * Example software running in Logisim RISC-V Computer System model by Pavel Gladyshev
 * licensed under Creative Commons Attribution International license 4.0
 *
 * This file defines process structure and related items
 */

#pragma once

#include "trap.h"

enum process_state 
{
    UNUSED,
    RUNNING
};

typedef struct process {
    int pid;  // process ID
    int state;
    trap_frame_t tf;
} process_t;