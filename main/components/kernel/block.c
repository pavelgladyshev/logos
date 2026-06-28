/*
 * Block device operations and block allocation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "block.h"
#include "fs_types.h"
#include "esp_err.h"
#include "esp_partition.h"
#include <string.h>
#include <sys/types.h>
#include "fs.h"
#include "fs_globals.h"
#include <stdint.h>

static const esp_partition_t* logosfs;
#define FLASH_SECTOR_SIZE 4096
static uint8_t sector_buf[FLASH_SECTOR_SIZE];

int block_init(void){

    logosfs = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            0x40,
            "logosfs"
            );
    if(!logosfs){
        return FS_ERR_IO;
    }

    return FS_OK;
}




int block_read(uint32_t block_num, void *buf) {
    uint32_t offset = block_num * BLOCK_SIZE;

    if(!logosfs || !buf){
        return FS_ERR_IO;
    }

    if(offset + BLOCK_SIZE > logosfs->size){
        return FS_ERR_IO;
    }

    return 
        esp_partition_read(logosfs, offset, buf, BLOCK_SIZE) == ESP_OK
        ? FS_OK
        : FS_ERR_IO;

    
}

int block_write(uint32_t block_num, const void *buf) {

    uint32_t block_offset = block_num * BLOCK_SIZE;
    uint32_t sector_offset = (block_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    uint32_t within_sector = block_offset - sector_offset;

    if(!logosfs || !buf){
        return FS_ERR_IO;
    }
    if(block_offset + BLOCK_SIZE > logosfs->size){
    return FS_ERR_IO;
    }

    if(esp_partition_read(logosfs, sector_offset, sector_buf, FLASH_SECTOR_SIZE) != ESP_OK){
        return FS_ERR_IO;
    }

    memcpy(sector_buf + within_sector, buf, BLOCK_SIZE);

    
    if(esp_partition_erase_range(logosfs, sector_offset, FLASH_SECTOR_SIZE) != ESP_OK){
        return FS_ERR_IO;
    }

    if(esp_partition_write(logosfs, sector_offset, sector_buf, FLASH_SECTOR_SIZE) != ESP_OK){
        return FS_ERR_IO;
    }

    return FS_OK;
}


int block_count(void){
    if(!logosfs){
        return 0;
    }
    else{
        return logosfs->size / BLOCK_SIZE;
    }
}

