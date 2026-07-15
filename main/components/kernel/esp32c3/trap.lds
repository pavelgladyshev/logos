/*
 * logOS ESP32-C3 trap-entry placement.
 *
 * This is a supplemental linker script: ESP-IDF still owns the complete
 * ESP32-C3 memory layout. trap.S uses ESP-IDF's .exception_vectors.text input
 * section, which the generated sections.ld places in IRAM while also moving
 * the start of DRAM past the same physical SRAM. Keeping placement in that
 * standard output section is essential because IRAM and DRAM are aliases.
 *
 * The boundary labels are emitted by trap.S. This supplemental script exports
 * their difference and enforces a fixed IRAM budget.
 */

_logos_trap_size = _logos_trap_end - _logos_trap_start;

/* The current handler is about 572 bytes; leave room for modest growth. */
ASSERT(_logos_trap_size <= 1K,
       "logOS ESP32-C3 trap handler exceeds its 1 KiB IRAM budget");
