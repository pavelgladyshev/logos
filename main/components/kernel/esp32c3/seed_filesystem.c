#include "seed_filesystem.h"

#include <stdint.h>

#include "file.h"
#include "fs.h"
#include "fs_types.h"
#include "console.h"

extern const uint8_t shell_elf_start[] asm("_binary_logos_shell_elf_start");
extern const uint8_t shell_elf_end[] asm("_binary_logos_shell_elf_end");
extern const uint8_t hello_elf_start[] asm("_binary_logos_hello_elf_start");
extern const uint8_t hello_elf_end[] asm("_binary_logos_hello_elf_end");
extern const uint8_t cat_elf_start[] asm("_binary_logos_cat_elf_start");
extern const uint8_t cat_elf_end[] asm("_binary_logos_cat_elf_end");
extern const uint8_t cp_elf_start[] asm("_binary_logos_cp_elf_start");
extern const uint8_t cp_elf_end[] asm("_binary_logos_cp_elf_end");
extern const uint8_t ed_elf_start[] asm("_binary_logos_ed_elf_start");
extern const uint8_t ed_elf_end[] asm("_binary_logos_ed_elf_end");
extern const uint8_t env_demo_elf_start[] asm("_binary_logos_env_demo_elf_start");
extern const uint8_t env_demo_elf_end[] asm("_binary_logos_env_demo_elf_end");
extern const uint8_t fd_test_elf_start[] asm("_binary_logos_fd_test_elf_start");
extern const uint8_t fd_test_elf_end[] asm("_binary_logos_fd_test_elf_end");
extern const uint8_t fork_demo_elf_start[] asm("_binary_logos_fork_demo_elf_start");
extern const uint8_t fork_demo_elf_end[] asm("_binary_logos_fork_demo_elf_end");
extern const uint8_t kill_elf_start[] asm("_binary_logos_kill_elf_start");
extern const uint8_t kill_elf_end[] asm("_binary_logos_kill_elf_end");
extern const uint8_t ln_elf_start[] asm("_binary_logos_ln_elf_start");
extern const uint8_t ln_elf_end[] asm("_binary_logos_ln_elf_end");
extern const uint8_t ls_elf_start[] asm("_binary_logos_ls_elf_start");
extern const uint8_t ls_elf_end[] asm("_binary_logos_ls_elf_end");
extern const uint8_t mkdir_elf_start[] asm("_binary_logos_mkdir_elf_start");
extern const uint8_t mkdir_elf_end[] asm("_binary_logos_mkdir_elf_end");
extern const uint8_t mknod_elf_start[] asm("_binary_logos_mknod_elf_start");
extern const uint8_t mknod_elf_end[] asm("_binary_logos_mknod_elf_end");
extern const uint8_t mv_elf_start[] asm("_binary_logos_mv_elf_start");
extern const uint8_t mv_elf_end[] asm("_binary_logos_mv_elf_end");
extern const uint8_t pipe_demo_elf_start[] asm("_binary_logos_pipe_demo_elf_start");
extern const uint8_t pipe_demo_elf_end[] asm("_binary_logos_pipe_demo_elf_end");
extern const uint8_t pipe_test_elf_start[] asm("_binary_logos_pipe_test_elf_start");
extern const uint8_t pipe_test_elf_end[] asm("_binary_logos_pipe_test_elf_end");
extern const uint8_t ps_elf_start[] asm("_binary_logos_ps_elf_start");
extern const uint8_t ps_elf_end[] asm("_binary_logos_ps_elf_end");
extern const uint8_t redir_test_elf_start[] asm("_binary_logos_redir_test_elf_start");
extern const uint8_t redir_test_elf_end[] asm("_binary_logos_redir_test_elf_end");
extern const uint8_t rm_elf_start[] asm("_binary_logos_rm_elf_start");
extern const uint8_t rm_elf_end[] asm("_binary_logos_rm_elf_end");
extern const uint8_t rmdir_elf_start[] asm("_binary_logos_rmdir_elf_start");
extern const uint8_t rmdir_elf_end[] asm("_binary_logos_rmdir_elf_end");
extern const uint8_t spawn_demo_elf_start[] asm("_binary_logos_spawn_demo_elf_start");
extern const uint8_t spawn_demo_elf_end[] asm("_binary_logos_spawn_demo_elf_end");

struct seed_program {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
};

static const struct seed_program seed_programs[] = {
    {"hello",      hello_elf_start,      hello_elf_end},
    {"sh",         shell_elf_start,      shell_elf_end},
    {"cat",        cat_elf_start,        cat_elf_end},
    {"cp",         cp_elf_start,         cp_elf_end},
    {"ed",         ed_elf_start,         ed_elf_end},
    {"env_demo",   env_demo_elf_start,   env_demo_elf_end},
    {"fd_test",    fd_test_elf_start,    fd_test_elf_end},
    {"fork_demo",  fork_demo_elf_start,  fork_demo_elf_end},
    {"kill",       kill_elf_start,       kill_elf_end},
    {"ln",         ln_elf_start,         ln_elf_end},
    {"ls",         ls_elf_start,         ls_elf_end},
    {"mkdir",      mkdir_elf_start,      mkdir_elf_end},
    {"mknod",      mknod_elf_start,      mknod_elf_end},
    {"mv",         mv_elf_start,         mv_elf_end},
    {"pipe_demo",  pipe_demo_elf_start,  pipe_demo_elf_end},
    {"pipe_test",  pipe_test_elf_start,  pipe_test_elf_end},
    {"ps",         ps_elf_start,         ps_elf_end},
    {"redir_test", redir_test_elf_start, redir_test_elf_end},
    {"rm",         rm_elf_start,         rm_elf_end},
    {"rmdir",      rmdir_elf_start,      rmdir_elf_end},
    {"spawn_demo", spawn_demo_elf_start, spawn_demo_elf_end},
};
static int install_file(uint32_t parent_ino, const char *name,
                        const uint8_t *data, uint32_t size)
{
    int file_ino;
    int written;

    if (size > DIRECT_BLOCKS * BLOCK_SIZE) {
        return FS_ERR_NO_SPACE;
    }

    file_ino = file_create(parent_ino, name);
    if (file_ino < 0) {
        return file_ino;
    }

    written = file_write((uint32_t)file_ino, 0, data, size);
    if (written < 0) {
        return written;
    }
    if ((uint32_t)written != size) {
        return FS_ERR_NO_SPACE;
    }

    return FS_OK;
}

int logos_seed_filesystem(void)
{
    uint32_t ino;
    uint32_t i;
    int bin_ino;
    int result;

    result = fs_open("/bin", &ino);
    if (result == FS_OK) {
        bin_ino = (int)ino;
    } else if (result == FS_ERR_NOT_FOUND) {
        bin_ino = fs_mkdir(ROOT_INODE, "bin");
        if (bin_ino < 0) {
            return bin_ino;
        }
    } else {
        logos_printf("seeding error\n");
        return result;
    }

    /* Preserve installed programs and add whichever ones are missing. */
    for (i = 0; i < sizeof(seed_programs) / sizeof(seed_programs[0]); i++) {
        char path[MAX_FILENAME + 6] = "/bin/";
        uint32_t j = 5;
        const char *name = seed_programs[i].name;

        while (*name && j < sizeof(path) - 1) {
            path[j++] = *name++;
        }
        path[j] = '\0';

        result = fs_open(path, &ino);
        if (result == FS_ERR_NOT_FOUND) {
            uint32_t size = (uint32_t)(seed_programs[i].end - seed_programs[i].start);
            result = install_file((uint32_t)bin_ino, seed_programs[i].name,
                                  seed_programs[i].start, size);
            if (result != FS_OK) {
                return result;
            }
        } else if (result != FS_OK) {
            return result;
        }
    }

    return FS_OK;
}
