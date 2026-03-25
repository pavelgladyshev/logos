/*
 * Bootloader main: reads kernel from filesystem and loads into RAM
 * Licensed under Creative Commons Attribution International License 4.0
 */

#include "boot.h"

/* Bootloader global state (in BSS, placed at high RAM by boot.lds) */
static struct superblock sb;
static uint8_t block_buf[BLOCK_SIZE];

void boot_main(void)
{
    uint32_t kernel_ino;
    struct inode kernel_in;
    uint32_t file_size;
    uint32_t loaded;
    uint8_t *dst;

    /* Step 1: Read superblock and validate */
    if (boot_block_read(0, &sb) != 0 || sb.magic != FS_MAGIC) {
        boot_puts("BOOT: No filesystem\n");
        boot_halt();
    }

    /* Step 2: Resolve path /boot/kernel */
    if (boot_resolve_path(KERNEL_PATH, &sb, block_buf, &kernel_ino) != 0) {
        boot_puts("BOOT: Kernel not found\n");
        boot_halt();
    }

    /* Step 3: Read the kernel inode to get file size */
    {
        uint32_t inode_block = sb.inode_start + (kernel_ino / INODES_PER_BLOCK);
        uint32_t inode_offset = (kernel_ino % INODES_PER_BLOCK) * INODE_SIZE;

        if (boot_block_read(inode_block, block_buf) != 0) {
            boot_puts("BOOT: Read error\n");
            boot_halt();
        }
        boot_memcpy(&kernel_in, block_buf + inode_offset, INODE_SIZE);
    }

    file_size = kernel_in.size;
    if (file_size == 0) {
        boot_puts("BOOT: Kernel empty\n");
        boot_halt();
    }

    /* Step 4: Load kernel data block-by-block into RAM at KERNEL_LOAD_ADDR */
    dst = (uint8_t *)KERNEL_LOAD_ADDR;
    loaded = 0;

    {
        int b;
        /* Load direct blocks */
        for (b = 0; b < DIRECT_BLOCKS && loaded < file_size; b++) {
            uint32_t chunk;

            if (kernel_in.blocks[b] == 0)
                break;

            chunk = file_size - loaded;
            if (chunk > BLOCK_SIZE)
                chunk = BLOCK_SIZE;

            if (boot_block_read(kernel_in.blocks[b], dst) != 0) {
                boot_puts("BOOT: Read error\n");
                boot_halt();
            }

            dst += BLOCK_SIZE;
            loaded += chunk;
        }
        /* Load indirect blocks if needed */
        if (loaded < file_size && kernel_in.indirect != 0) {
            static uint16_t ind_entries[BLOCK_SIZE / sizeof(uint16_t)];
            int i;

            if (boot_block_read(kernel_in.indirect, ind_entries) != 0) {
                boot_puts("BOOT: Read error\n");
                boot_halt();
            }
            for (i = 0; i < (int)(BLOCK_SIZE / sizeof(uint16_t)) && loaded < file_size; i++) {
                uint32_t chunk;

                if (ind_entries[i] == 0)
                    break;

                chunk = file_size - loaded;
                if (chunk > BLOCK_SIZE)
                    chunk = BLOCK_SIZE;

                if (boot_block_read(ind_entries[i], dst) != 0) {
                    boot_puts("BOOT: Read error\n");
                    boot_halt();
                }

                dst += BLOCK_SIZE;
                loaded += chunk;
            }
        }
    }

    /* Step 5: Jump to kernel entry point */
    {
        void (*kernel_entry)(void) = (void (*)(void))KERNEL_LOAD_ADDR;
        kernel_entry();
    }

    boot_halt();
}
