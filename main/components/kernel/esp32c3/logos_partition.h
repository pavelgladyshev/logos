#pragma once

#include <stdint.h>

/*
 * Fixed flash window reserved for the logOS filesystem.
 * This mirrors partitions.csv but does not depend on ESP-IDF's partition list.
 */
#define LOGOS_PARTITION_OFFSET 0x110000U
#define LOGOS_PARTITION_SIZE   (256U * 1024U)
#define LOGOS_PARTITION_SECTOR_SIZE 4096U

/* Thin storage API used by block.c. Offsets are relative to LOGOS_PARTITION_OFFSET. */
int logos_partition_init(void);
int logos_partition_read(uint32_t offset, void *buf, uint32_t len);
int logos_partition_write(uint32_t offset, const void *buf, uint32_t len);
int logos_partition_erase(uint32_t offset, uint32_t len);
uint32_t logos_partition_size(void);
