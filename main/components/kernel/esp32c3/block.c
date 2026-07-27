/*
 * Block device operations and block allocation
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "block.h"
#include "console.h"
#include "fs_types.h"
#include "logos_partition.h"
#include <string.h>
#include <sys/types.h>
#include "fs.h"
#include "fs_globals.h"
#include <stdint.h>

#define CACHED_BLOCKS 8

static int logosfs_initialized;
static uint8_t sector_buf[LOGOS_PARTITION_SECTOR_SIZE] ;

static uint32_t cached_sector_offset = 0;
static int used = 0;
static int notUsed = 0;
static uint32_t blockNumQ[CACHED_BLOCKS]= {0};
static uint8_t blocksQ[BLOCK_SIZE * CACHED_BLOCKS] = {0};
int blockNumQFront =0;

/* Initialize the fixed flash-backed block device used by the filesystem. */
int block_init(void){
    logos_printf("Block init called\n");
    if (logos_partition_init() != 0) {
        logosfs_initialized = 0;
        return FS_ERR_IO;
    }

    //caching the first sector
    if(logos_partition_read(0, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
        return FS_ERR_IO;
    }

    logosfs_initialized = 1;
    return FS_OK;
}




int block_read(uint32_t block_num, void *buf) {
    //logos_printf("in block read %d\n",block_num);
    uint32_t offset = block_num * BLOCK_SIZE;

    uint32_t sector_offset = (offset / LOGOS_PARTITION_SECTOR_SIZE) * LOGOS_PARTITION_SECTOR_SIZE;
    uint32_t within_sector = offset-sector_offset;


    if(!logosfs_initialized || !buf){
        return FS_ERR_IO;
    }

    if(offset + BLOCK_SIZE > logos_partition_size()){
        return FS_ERR_IO;
    }

    //testing to see when caching could be used
    if(cached_sector_offset == sector_offset){
        //logos_printf("use: %d, not use: %d\n",++used,notUsed );
        memcpy(buf, sector_buf+within_sector, BLOCK_SIZE);
        return FS_OK;
    }
    else{
        int succ = 0;
        for(int i =0;i<CACHED_BLOCKS;i++){
            if(blockNumQ[i] == block_num){
                //logos_printf("reading cached blocks\n");
                //logos_printf("use: %d, not use: %d\n",++used,notUsed );
                memcpy(buf, &(blocksQ[i*BLOCK_SIZE]),BLOCK_SIZE);
                succ = 1;
                return FS_OK;
            }
        }
        if(!succ){
            //logos_printf("adding to cached blocks\n");
            //logos_printf("use: %d, not use: %d\n",used,++notUsed );
            blockNumQ[blockNumQFront%CACHED_BLOCKS] = block_num; 
            logos_partition_read(offset,&(blocksQ[(blockNumQFront%CACHED_BLOCKS) * BLOCK_SIZE]) , BLOCK_SIZE);

            blockNumQFront++;
        }
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

    //logos_printf("in block write\n");

    uint32_t block_offset = block_num * BLOCK_SIZE;
    uint32_t sector_offset = (block_offset / LOGOS_PARTITION_SECTOR_SIZE) * LOGOS_PARTITION_SECTOR_SIZE;
    uint32_t within_sector = block_offset - sector_offset;
    

    if(!logosfs_initialized || !buf){
        return FS_ERR_IO;
    }
    if(block_offset + BLOCK_SIZE > logos_partition_size()){
        return FS_ERR_IO;
    }

    //testing to see when caching could be used
    //if we need to write to the cached sector we can simply write to the cache
    if(cached_sector_offset == sector_offset){
        //logos_printf("use: %d, not use: %d\n",++used,notUsed );
        memcpy(sector_buf + within_sector, buf, BLOCK_SIZE);
    }
    //if we need to write to a section that isn't in the cache we need to
    //write the cached section
    //read the new section into cache
    //write to that section
    else{
        //writing cached section
        if(logos_partition_erase(cached_sector_offset, LOGOS_PARTITION_SECTOR_SIZE) != 0){
            return FS_ERR_IO;
        }

        if(logos_partition_write(cached_sector_offset, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
            return FS_ERR_IO;
        }

        //need to check if any of the cached blocks are from within the sector, and if they are writing to them 
        for(int i = 0;i<CACHED_BLOCKS;i++){
            if(blockNumQ[i] * BLOCK_SIZE >= cached_sector_offset && blockNumQ[i]*BLOCK_SIZE < cached_sector_offset + LOGOS_PARTITION_SECTOR_SIZE){
                //logos_printf("cached block within sector so rewritting\n");
                uint32_t blockNumWithinSector = (blockNumQ[i] * BLOCK_SIZE - cached_sector_offset) / BLOCK_SIZE;

                memcpy(&(blocksQ[i*BLOCK_SIZE]), &(sector_buf[blockNumWithinSector*BLOCK_SIZE]), BLOCK_SIZE); 

            }

        }
        

        //reading the new section
        if(logos_partition_read(sector_offset, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
            return FS_ERR_IO;
        }
        //writing to that section
        memcpy(sector_buf + within_sector, buf, BLOCK_SIZE);
        //logos_printf("switched cache\n");
        //logos_printf("use: %d, not use: %d\n",++used,notUsed );
        cached_sector_offset = sector_offset;
    }

    //if(logos_partition_read(sector_offset, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
    //    return FS_ERR_IO;
    //}

    //memcpy(sector_buf + within_sector, buf, BLOCK_SIZE);

    //
    //if(logos_partition_erase(sector_offset, LOGOS_PARTITION_SECTOR_SIZE) != 0){
    //    return FS_ERR_IO;
    //}

    //if(logos_partition_write(sector_offset, sector_buf, LOGOS_PARTITION_SECTOR_SIZE) != 0){
    //    return FS_ERR_IO;
    //}

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
