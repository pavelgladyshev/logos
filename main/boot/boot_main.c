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

    boot_puts("Bootloader starting...\n");

    /* Step 1: Read superblock and validate */
    if (boot_block_read(0, &sb) != 0) {
        boot_puts("BOOT: Failed to read superblock\n");
        boot_halt();
    }
    if (sb.magic != FS_MAGIC) {
        boot_puts("BOOT: Invalid filesystem magic\n");
        boot_halt();
    }

    boot_puts("BOOT: Filesystem found\n");

    /* Step 2: Resolve path /boot/kernel */
    if (boot_resolve_path(KERNEL_PATH, &sb, block_buf, &kernel_ino) != 0) {
        boot_puts("BOOT: Kernel not found at ");
        boot_puts(KERNEL_PATH);
        boot_puts("\n");
        boot_halt();
    }

    boot_puts("BOOT: Found kernel at inode ");
    boot_put_uint(kernel_ino);
    boot_puts("\n");

    /* Step 3: Read the kernel inode to get file size */
    {
        uint32_t inode_block = sb.inode_start + (kernel_ino / INODES_PER_BLOCK);
        uint32_t inode_offset = (kernel_ino % INODES_PER_BLOCK) * INODE_SIZE;

        if (boot_block_read(inode_block, block_buf) != 0) {
            boot_puts("BOOT: Failed to read kernel inode\n");
            boot_halt();
        }
        boot_memcpy(&kernel_in, block_buf + inode_offset, INODE_SIZE);
    }

    file_size = kernel_in.size;
    boot_puts("BOOT: Kernel size ");
    boot_put_uint(file_size);
    boot_puts(" bytes\n");

    if (file_size == 0) {
        boot_puts("BOOT: Kernel file is empty\n");
        boot_halt();
    }

    /* Step 4: Load kernel data block-by-block into RAM at KERNEL_LOAD_ADDR */
    dst = (uint8_t *)KERNEL_LOAD_ADDR;
    loaded = 0;

    {
        int b;
        for (b = 0; b < DIRECT_BLOCKS && loaded < file_size; b++) {
            uint32_t chunk;

            if (kernel_in.blocks[b] == 0)
                break;

            chunk = file_size - loaded;
            if (chunk > BLOCK_SIZE)
                chunk = BLOCK_SIZE;

            /* DMA directly to destination in RAM */
            if (boot_block_read(kernel_in.blocks[b], dst) != 0) {
                boot_puts("BOOT: Failed to read kernel block\n");
                boot_halt();
            }

            dst += BLOCK_SIZE;
            loaded += chunk;
        }
    }

    boot_puts("BOOT: Loaded ");
    boot_put_uint(loaded);
    boot_puts(" bytes to 0x");
    boot_put_hex(KERNEL_LOAD_ADDR);
    boot_puts("\n");

    boot_puts("BOOT: Jumping to kernel at 0x");
    boot_put_hex(KERNEL_LOAD_ADDR);
    boot_puts("\n\n");

    /* Step 5: Jump to kernel entry point */
    {
        void (*kernel_entry)(void) = (void (*)(void))KERNEL_LOAD_ADDR;
        kernel_entry();
    }

    /* Should never reach here */
    boot_halt();
}
