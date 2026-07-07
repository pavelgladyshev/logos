/*
 * Block device operations and block allocation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "block.h"
#include "fs_types.h"
#include "logos_partition.h"
#include <string.h>
#include <sys/types.h>
#include "fs.h"
#include "fs_globals.h"
#include <stdint.h>

static int logosfs_initialized;
static uint8_t sector_buf[LOGOS_PARTITION_SECTOR_SIZE];

/* Initialize the fixed flash-backed block device used by the filesystem. */
int block_init(void){
    if (logos_partition_init() != 0) {
        logosfs_initialized = 0;
        return FS_ERR_IO;
    }

    logosfs_initialized = 1;
    return FS_OK;
}




int block_read(uint32_t block_num, void *buf) {
    uint32_t offset = block_num * BLOCK_SIZE;

    if(!logosfs_initialized || !buf){
        return FS_ERR_IO;
    }

    if(offset + BLOCK_SIZE > logos_partition_size()){
        return FS_ERR_IO;
    }

    return 
        logos_partition_read(offset, buf, BLOCK_SIZE) == 0
        ? FS_OK
        : FS_ERR_IO;

    
}

/*
 * Flash can only be erased by sector, while the filesystem writes 512-byte
 * blocks. Preserve the rest of the sector with a read-modify-erase-write cycle.
 */
int block_write(uint32_t block_num, const void *buf) {

    uint32_t block_offset = block_num * BLOCK_SIZE;
    uint32_t sector_offset = (block_offset / LOGOS_PARTITION_SECTOR_SIZE) * LOGOS_PARTITION_SECTOR_SIZE;
    uint32_t within_sector = block_offset - sector_offset;

    if(!logosfs_initialized || !buf){
        return FS_ERR_IO;
    }
    if(block_offset + BLOCK_SIZE > logos_partition_size()){
    return FS_ERR_IO;
    }

    if(logos_partition_read(sector_offset, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
        return FS_ERR_IO;
    }

    memcpy(sector_buf + within_sector, buf, BLOCK_SIZE);

    
    if(logos_partition_erase(sector_offset, LOGOS_PARTITION_SECTOR_SIZE) != 0){
        return FS_ERR_IO;
    }

    if(logos_partition_write(sector_offset, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
        return FS_ERR_IO;
    }

    return FS_OK;
}


int block_count(void){
    if(!logosfs_initialized){
        return 0;
    }
    else{
        return logos_partition_size() / BLOCK_SIZE;
    }
}
