#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "block.h"
#include "file.h"
#include "fs.h"
#include "fs_types.h"

static void seed_filesystem(void)
{
    int etc_ino;
    int file_ino;
    const char *msg = "hello from logOS on ESP\n";

    etc_ino = fs_mkdir(ROOT_INODE, "etc");
    if (etc_ino < 0) {
        printf("mkdir /etc failed: %d\n", etc_ino);
        return;
    }

    file_ino = file_create((uint32_t)etc_ino, "hello.txt");
    if (file_ino < 0) {
        printf("create hello.txt failed: %d\n", file_ino);
        return;
    }

    if (file_write((uint32_t)file_ino, 0, msg, strlen(msg)) < 0) {
        printf("write hello.txt failed\n");
        return;
    }

    printf("seeded /etc/hello.txt\n");
}


static void print_hello_file(void){
    uint32_t ino;
    char buf[64];
    int n;

    if(fs_open("/etc/hello.txt", &ino) != FS_OK){
        printf("Failed to open /etc/hello.txt");
        return;
    }

    n = file_read(ino, 0, buf, sizeof(buf) - 1);
    if(n < 0){
        printf("Failed to read /etc/hello.txt");
        return;
    }

    buf[n] = '\0';
    printf("/etc/hello.txt: %s", buf);
}


void logos_start(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("logos_start\n");
    fflush(stdout);

    while (block_init() != FS_OK) {
        printf("block_init failed: logosfs partition not found\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("block_init OK\n");

    if (fs_mount() != FS_OK) {
        uint32_t blocks = block_count();

        printf("logOS filesystem not found, formatting %lu blocks\n", (unsigned long)blocks);

        if (fs_format(blocks) != FS_OK) {
            printf("format failed\n");
        } else {
            printf("format succeeded\n");
            seed_filesystem();

            if (fs_mount() == FS_OK) {
                printf("filesystem mounted\n");
            } else {
                printf("failed to mount filesystem after format\n");
            }
        }
    } else {
        printf("filesystem mounted\n");
    }
    print_hello_file();
    fflush(stdout);
}
