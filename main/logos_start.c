#include <stdint.h>

#include "block.h"
#include "console.h"
#include "file.h"
#include "fs.h"
#include "fs_types.h"
#include "kernel_time.h"

/* Local strlen avoids depending on stdio/newlib-facing helpers in startup code. */
static int logos_strlen(const char *s)
{
    int len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len;
}

/* Create a tiny known file so the shell/demo has persistent content to read. */
static void seed_filesystem(void)
{
    int etc_ino;
    int file_ino;
    const char *msg = "hello from logOS on ESP\n";

    etc_ino = fs_mkdir(ROOT_INODE, "etc");
    if (etc_ino < 0) {
        kernel_console_printf("mkdir /etc failed: %d\n", etc_ino);
        return;
    }

    file_ino = file_create((uint32_t)etc_ino, "hello.txt");
    if (file_ino < 0) {
        kernel_console_printf("create hello.txt failed: %d\n", file_ino);
        return;
    }

    if (file_write((uint32_t)file_ino, 0, msg, logos_strlen(msg)) < 0) {
        kernel_console_printf("write hello.txt failed\n");
        return;
    }

    kernel_console_printf("seeded /etc/hello.txt\n");
}


/* Smoke-test the filesystem after mount or format. */
static void print_hello_file(void){
    uint32_t ino;
    char buf[64];
    int n;

    if(fs_open("/etc/hello.txt", &ino) != FS_OK){
        kernel_console_printf("Failed to open /etc/hello.txt");
        return;
    }

    n = file_read(ino, 0, buf, sizeof(buf) - 1);
    if(n < 0){
        kernel_console_printf("Failed to read /etc/hello.txt");
        return;
    }

    buf[n] = '\0';
    kernel_console_printf("/etc/hello.txt: %s", buf);
}


void logos_start(void)
{
    /*
     * Give the ROM monitor/UART side a short settling window after boot before
     * logOS starts writing its own status messages.
     */
    kernel_delay_ms(1000);

    kernel_console_printf("logos_start\n");

    /*
     * The block layer owns fixed flash storage. If it cannot initialize, there
     * is no useful filesystem work to do, so retry instead of continuing.
     */
    while (block_init() != FS_OK) {
        kernel_console_printf("block_init failed: logosfs partition not found\n");
        kernel_delay_ms(1000);
    }

    kernel_console_printf("block_init OK\n");

    if (fs_mount() != FS_OK) {
        uint32_t blocks = block_count();

        /* First boot, erased flash, or incompatible on-flash layout. */
        kernel_console_printf("logOS filesystem not found, formatting %lu blocks\n", (unsigned long)blocks);

        if (fs_format(blocks) != FS_OK) {
            kernel_console_printf("format failed\n");
        } else {
            kernel_console_printf("format succeeded\n");
            seed_filesystem();

            if (fs_mount() == FS_OK) {
                kernel_console_printf("filesystem mounted\n");
            } else {
                kernel_console_printf("failed to mount filesystem after format\n");
            }
        }
    } else {
        kernel_console_printf("filesystem mounted\n");
    }
    print_hello_file();
}
