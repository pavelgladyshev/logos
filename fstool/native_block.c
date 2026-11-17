/*
 * Native backing store operations using standard C file I/O
 * For use with the fstool native utility and the FUSE driver logosfs
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * block_read() and block_write() here take the place of the kernel's, which
 * drive the block storage device: they read and write a disk image file.
 * Everything else (bitmap, block_alloc, block_free) is kernel/block.c's.
 *
 * The "image" can also be the serial port of a Tang Nano 20K that runs the
 * book's sdcard-bridge bitstream: the blocks are then the microSD card's.
 */

#include <stdio.h>
#include <string.h>
#include "fs_types.h"
#include "fs_globals.h"
#include "native_block.h"

/* Global file handle for the block storage file */
static FILE *block_file = NULL;
static char block_filename[256] = "";

/* ------------------------------------------------------------------------------------------------
 * The card in a Tang Nano 20K, through its USB serial port
 *
 * When the "image" is a character device (/dev/ttyUSB1 on Linux, /dev/cu.usbserial-...1 on macOS),
 * it is the serial port of a Tang Nano 20K that runs the sdbridge bitstream of the book's supplement
 * (sdcard-bridge/), and block n is block n of the microSD card in the board's TF slot. fstool then
 * formats and fills the card itself, and logosfs mounts it. Requests (115200 baud, 8N1):
 *   'P'                         -> "SDB1"
 *   'R' block[4]                -> 'K' data[512] sum[2]   or 'E'
 *   'W' block[4] data[512] sum[2] -> 'K'  or 'E' (the card failed)  or 'C' (damaged on the way)
 * Numbers are little-endian; sum is the 16-bit sum of the data bytes. Blocks read are kept in memory,
 * so that the file system's many reads of the same few blocks cross the serial line once: no one else
 * can change the card while the board's serial port is held open here.
 * ---------------------------------------------------------------------------------------------- */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

static int serial_fd = -1;
static uint8_t **serial_cache = NULL;      /* serial_cache[n]: block n as last read or written */
static uint32_t serial_cache_size = 0;

static double serial_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* Read exactly n bytes, or fewer if timeout seconds pass first; returns how many were read */
static int serial_read(uint8_t *buf, int n, double timeout) {
    int got = 0;
    double end = serial_now() + timeout;
    while (got < n) {
        double left = end - serial_now();
        if (left <= 0) break;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serial_fd, &fds);
        struct timeval tv = { (long)left, (long)((left - (long)left) * 1e6) };
        if (select(serial_fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        ssize_t r = read(serial_fd, buf + got, n - got);
        if (r > 0) got += r;
        else if (r < 0 && errno != EAGAIN && errno != EINTR) break;
    }
    return got;
}

static int serial_write(const uint8_t *buf, int n) {
    while (n > 0) {
        ssize_t w = write(serial_fd, buf, n);
        if (w > 0) { buf += w; n -= w; }
        else if (w < 0 && errno != EAGAIN && errno != EINTR) return -1;
        else { fd_set fds; FD_ZERO(&fds); FD_SET(serial_fd, &fds); select(serial_fd + 1, NULL, &fds, NULL, NULL); }
    }
    return 0;
}

/* Bring the board back to waiting for a command, and check that it is the bridge */
static int serial_resync(void) {
    uint8_t id[4], junk[256];
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt) usleep(600000);          /* an unfinished request ends after half a second */
        tcflush(serial_fd, TCIOFLUSH);
        while (serial_read(junk, sizeof junk, 0.05) > 0) {}
        if (serial_write((const uint8_t *)"P", 1) == 0 && serial_read(id, 4, 1.0) == 4 &&
            memcmp(id, "SDB1", 4) == 0)
            return 0;
    }
    return -1;
}

static void serial_le32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

static uint16_t serial_sum(const uint8_t *data) {
    uint16_t s = 0;
    for (int i = 0; i < BLOCK_SIZE; i++) s += data[i];
    return s;
}

static int serial_open(const char *filename) {
    serial_fd = open(filename, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd < 0) return -1;
    struct termios t;
    if (tcgetattr(serial_fd, &t) != 0) { close(serial_fd); serial_fd = -1; return -1; }
    cfmakeraw(&t);
    t.c_cflag |= CLOCAL | CREAD;
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    tcsetattr(serial_fd, TCSANOW, &t);
    if (serial_resync() != 0) {
        fprintf(stderr, "%s: no sdbridge answers (is its bitstream loaded into the board?)\n", filename);
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }
    return 0;
}

static uint8_t *serial_cached(uint32_t block_num) {
    if (block_num < serial_cache_size && serial_cache[block_num]) return serial_cache[block_num];
    return NULL;
}

static void serial_keep(uint32_t block_num, const void *buf) {
    if (block_num >= serial_cache_size) {
        uint32_t size = block_num + 1024;
        uint8_t **c = realloc(serial_cache, size * sizeof *c);
        if (c == NULL) return;
        memset(c + serial_cache_size, 0, (size - serial_cache_size) * sizeof *c);
        serial_cache = c;
        serial_cache_size = size;
    }
    if (serial_cache[block_num] == NULL && (serial_cache[block_num] = malloc(BLOCK_SIZE)) == NULL) return;
    memcpy(serial_cache[block_num], buf, BLOCK_SIZE);
}

static int serial_block_read(uint32_t block_num, void *buf) {
    uint8_t *c = serial_cached(block_num);
    if (c) { memcpy(buf, c, BLOCK_SIZE); return FS_OK; }
    for (int attempt = 0; attempt < 4; attempt++) {
        uint8_t req[5] = { 'R' }, status = 0, reply[BLOCK_SIZE + 2];
        serial_le32(req + 1, block_num);
        if (serial_write(req, 5) == 0 && serial_read(&status, 1, 5.0) == 1) {
            if (status == 'E') return FS_ERR_IO;
            if (status == 'K' && serial_read(reply, BLOCK_SIZE + 2, 5.0) == BLOCK_SIZE + 2 &&
                serial_sum(reply) == (uint16_t)(reply[BLOCK_SIZE] | reply[BLOCK_SIZE + 1] << 8)) {
                memcpy(buf, reply, BLOCK_SIZE);
                serial_keep(block_num, reply);
                return FS_OK;
            }
        }
        if (serial_resync() != 0) break;
    }
    return FS_ERR_IO;
}

static int serial_block_write(uint32_t block_num, const void *buf) {
    uint8_t *c = serial_cached(block_num);
    if (c && memcmp(c, buf, BLOCK_SIZE) == 0) return FS_OK;     /* the card has it already */
    uint8_t req[5 + BLOCK_SIZE + 2] = { 'W' }, status = 0;
    serial_le32(req + 1, block_num);
    memcpy(req + 5, buf, BLOCK_SIZE);
    uint16_t s = serial_sum(buf);
    req[5 + BLOCK_SIZE] = s;
    req[6 + BLOCK_SIZE] = s >> 8;
    for (int attempt = 0; attempt < 4; attempt++) {
        if (serial_write(req, sizeof req) == 0 && serial_read(&status, 1, 5.0) == 1) {
            if (status == 'K') { serial_keep(block_num, buf); return FS_OK; }
            if (status == 'E') return FS_ERR_IO;
        }
        if (serial_resync() != 0) break;
    }
    return FS_ERR_IO;
}

static void serial_close(void) {
    if (serial_fd >= 0) close(serial_fd);
    serial_fd = -1;
    for (uint32_t i = 0; i < serial_cache_size; i++) free(serial_cache[i]);
    free(serial_cache);
    serial_cache = NULL;
    serial_cache_size = 0;
}

static int is_serial_port(const char *filename) {
    struct stat st;
    return stat(filename, &st) == 0 && S_ISCHR(st.st_mode);
}

/*
 * Open the block storage file
 * Returns 0 on success, -1 on failure
 */
int native_backing_store_open(const char *filename) {
    native_backing_store_close();
    if (is_serial_port(filename)) {
        return serial_open(filename);
    }

    block_file = fopen(filename, "r+b");
    if (block_file == NULL) {
        /* Try to create the file if it doesn't exist */
        block_file = fopen(filename, "w+b");
        if (block_file == NULL) {
            return -1;
        }
    }

    strncpy(block_filename, filename, sizeof(block_filename) - 1);
    block_filename[sizeof(block_filename) - 1] = '\0';
    return 0;
}

/*
 * Create a new block storage file with the specified number of blocks
 * Returns 0 on success, -1 on failure
 */
int native_backing_store_create(const char *filename, uint32_t num_blocks) {
    if (is_serial_port(filename)) {
        /* A card keeps its blocks: fs_format writes every block that the file system reads */
        return native_backing_store_open(filename);
    }
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }

    /* Write zeros for all blocks */
    uint8_t zero_block[BLOCK_SIZE];
    memset(zero_block, 0, BLOCK_SIZE);

    for (uint32_t i = 0; i < num_blocks; i++) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, f) != 1) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    /* Now open it for read/write */
    return native_backing_store_open(filename);
}

/*
 * Close the block storage file
 */
void native_backing_store_close(void) {
    serial_close();
    if (block_file != NULL) {
        fclose(block_file);
        block_file = NULL;
    }
    block_filename[0] = '\0';
}

/*
 * Read a block from the storage file
 */
int block_read(uint32_t block_num, void *buf) {
    if (serial_fd >= 0) {
        return serial_block_read(block_num, buf);
    }
    if (block_file == NULL) {
        return FS_ERR_IO;
    }

    if (fseek(block_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        return FS_ERR_IO;
    }

    if (fread(buf, BLOCK_SIZE, 1, block_file) != 1) {
        /* Check if we're reading beyond end of file - return zeros */
        if (feof(block_file)) {
            memset(buf, 0, BLOCK_SIZE);
            return FS_OK;
        }
        return FS_ERR_IO;
    }

    return FS_OK;
}

/*
 * Write a block to the storage file
 */
int block_write(uint32_t block_num, const void *buf) {
    if (serial_fd >= 0) {
        return serial_block_write(block_num, buf);
    }
    if (block_file == NULL) {
        return FS_ERR_IO;
    }

    if (fseek(block_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        return FS_ERR_IO;
    }

    if (fwrite(buf, BLOCK_SIZE, 1, block_file) != 1) {
        return FS_ERR_IO;
    }

    /* Flush to ensure data is written */
    fflush(block_file);

    return FS_OK;
}
