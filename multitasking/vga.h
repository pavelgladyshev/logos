/*
 * Example software running in Logisim RISC-V Computer System model by Pavel Gladyshev
 * licensed under Creative Commons Attribution International license 4.0
 *
 * This file contains constants for memory mapped VGA display
 */

#pragma once

#define VGA ((volatile unsigned int*) 0xFFFF8000)
#define VGA_WIDTH (32)
#define VGA_HEIGHT (32)
