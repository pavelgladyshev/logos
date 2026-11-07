/*
 * logosfs - a LogOS disk image as a file system of the host (Linux or macOS), through FUSE
 * Licensed under Creative Commons Attribution International License 4.0
 *
 *     fstool/logosfs block_storage.bin mountpoint [FUSE options]
 *
 * The file system code is LogOS's own: kernel/fs.c, dir.c, file.c, inode.c and block.c, built for
 * the host as for fstool, with fstool/native_block.c reading and writing the image file in place of
 * the block storage device. Each operation below calls what the LogOS system call of the same job
 * calls (sys_open, sys_read, sys_write, sys_mkdir, sys_unlink, sys_link, sys_rename...), so a change
 * to LogOS's file system shows here too after "make logosfs".
 *
 * This is a file system driver of the host's kernel: FUSE (File system in USErspace) passes the
 * kernel's file operations on a mounted file system to this program, which answers them. LogOS's
 * kernel does the same job for its programs in syscall.c.
 *
 * FUSE runs it single-threaded (-s): the kernel code uses one block buffer (block_buf).
 */

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/sysmacros.h>                       /* makedev, major, minor */
#endif

#include "fs.h"
#include "fs_globals.h"
#include "native_block.h"

#define MAX_FILE_SIZE  ((off_t)(DIRECT_BLOCKS + INDIRECT_ENTRIES) * BLOCK_SIZE)
#define MAX_DIRENTS    (DIRECT_BLOCKS * DIRENTS_PER_BLOCK)

static time_t image_time;    /* LogOS keeps no times: every file shows the image file's */

/* LogOS's error codes as the host's (negated errno, as FUSE wants them) */
static int host_err(int fs_err)
{
    switch (fs_err) {
    case FS_ERR_NOT_FOUND: return -ENOENT;
    case FS_ERR_EXISTS:    return -EEXIST;
    case FS_ERR_NO_SPACE:  return -ENOSPC;
    case FS_ERR_NOT_DIR:   return -ENOTDIR;
    case FS_ERR_IS_DIR:    return -EISDIR;
    case FS_ERR_NOT_EMPTY: return -ENOTEMPTY;
    case FS_ERR_INVALID:   return -EINVAL;
    default:               return -EIO;
    }
}

/* fs_open() would cut a name longer than LogOS's 27 characters short and find another file */
static int check_names(const char *path)
{
    size_t len = 0;
    for (const char *p = path; ; p++) {
        if (*p == '/' || *p == '\0') {
            if (len > MAX_FILENAME - 1) return -ENAMETOOLONG;
            if (*p == '\0') return 0;
            len = 0;
        } else {
            len++;
        }
    }
}

/* fs_open() for the host: the inode of a path, or a negated errno */
static int lookup(const char *path, uint32_t *ino, struct inode *in)
{
    int r = check_names(path);
    if (r != 0) return r;
    r = fs_open(path, ino);
    if (r != FS_OK) return host_err(r);
    if (in != NULL && inode_read(*ino, in) != FS_OK) return -EIO;
    return 0;
}

/* fs_open_parent() for the host: the directory a new name goes in */
static int lookup_parent(const char *path, uint32_t *parent_ino, const char **name)
{
    int r = check_names(path);
    if (r != 0) return r;
    r = fs_open_parent(path, parent_ino, name);
    if (r == FS_ERR_INVALID && strlen(path) >= FS_MAX_PATH) return -ENAMETOOLONG;
    return r == FS_OK ? 0 : host_err(r);
}

/* LogOS cannot read past a block a file does not have (file_read stops there), so where the host
 * writes past the end of a file, or makes it longer, the gap is written with zeros */
static int fill_zeros(uint32_t ino, uint32_t from, uint32_t to)
{
    static const uint8_t zeros[BLOCK_SIZE];
    while (from < to) {
        uint32_t n = to - from < BLOCK_SIZE ? to - from : BLOCK_SIZE;
        int w = file_write(ino, from, zeros, n);
        if (w < 0) return host_err(w);
        if ((uint32_t)w < n) return to > MAX_FILE_SIZE ? -EFBIG : -ENOSPC;
        from += n;
    }
    return 0;
}

/* stat: like sys_stat (fs_open + inode_read), in the host's terms */
static int lfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi)
{
    (void)fi;
    uint32_t ino;
    struct inode in;
    int r = lookup(path, &ino, &in);
    if (r != 0) return r;

    memset(st, 0, sizeof(*st));
    st->st_ino = ino + 1;                        /* the host takes no inode 0: LogOS's root is 1 here */
    switch (in.type) {
    case FT_DIR:     st->st_mode = S_IFDIR | 0755; st->st_nlink = 2; break;
    case FT_CHARDEV: st->st_mode = S_IFCHR | 0666; st->st_nlink = in.link_count;
                     st->st_rdev = makedev(in.major, in.minor); break;
    default:         st->st_mode = S_IFREG | 0644; st->st_nlink = in.link_count; break;
    }
    st->st_size = in.type == FT_CHARDEV ? 0 : in.size;
    st->st_blksize = BLOCK_SIZE;
    st->st_blocks = (st->st_size + 511) / 512;
    st->st_uid = getuid();                       /* LogOS has no owners or permissions either */
    st->st_gid = getgid();
    st->st_atime = st->st_mtime = st->st_ctime = image_time;
    return 0;
}

/* readdir: like sys_readdir (fs_open + dir_list) */
static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t fill, off_t off,
                       struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void)off; (void)fi; (void)flags;
    static struct dirent entries[MAX_DIRENTS];
    uint32_t ino, count;
    int r = lookup(path, &ino, NULL);
    if (r != 0) return r;
    r = dir_list(ino, entries, MAX_DIRENTS, &count);
    if (r != FS_OK) return host_err(r);
    for (uint32_t i = 0; i < count && i < MAX_DIRENTS; i++) {
        fill(buf, entries[i].name, NULL, 0, 0);
    }
    return 0;
}

/* open: like sys_open without O_CREAT (fs_open); O_TRUNC comes as a truncate */
static int lfs_open(const char *path, struct fuse_file_info *fi)
{
    uint32_t ino;
    struct inode in;
    int r = lookup(path, &ino, &in);
    if (r != 0) return r;
    fi->direct_io = 1;                           /* each read and write comes here, as it is made */
    return in.type == FT_DIR ? -EISDIR : 0;
}

/* create: like sys_open with O_CREAT (fs_open_parent + file_create) */
static int lfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void)mode;
    fi->direct_io = 1;
    uint32_t parent;
    const char *name;
    int r = lookup_parent(path, &parent, &name);
    if (r != 0) return r;
    r = file_create(parent, name);
    return r < 0 ? host_err(r) : 0;
}

/* mknod: a regular file (file_create) or, like sys_mknod, a character device (fs_mknod) */
static int lfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    uint32_t parent;
    const char *name;
    int r = lookup_parent(path, &parent, &name);
    if (r != 0) return r;
    if (S_ISREG(mode)) r = file_create(parent, name);
    else if (S_ISCHR(mode)) r = fs_mknod(parent, name, (uint8_t)major(rdev), (uint8_t)minor(rdev));
    else return -EPERM;
    return r < 0 ? host_err(r) : 0;
}

/* read: like sys_read on a file (file_read) */
static int lfs_read(const char *path, char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
    (void)fi;
    uint32_t ino;
    struct inode in;
    int r = lookup(path, &ino, &in);
    if (r != 0) return r;
    if (off >= MAX_FILE_SIZE) return 0;
    r = file_read(ino, (uint32_t)off, buf, (uint32_t)size);
    if (r == FS_ERR_NOT_FOUND && in.type == FT_CHARDEV) return -ENODEV;   /* no LogOS devices here */
    return r < 0 ? host_err(r) : r;
}

/* write: like sys_write on a file (file_write) */
static int lfs_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
    (void)fi;
    uint32_t ino;
    struct inode in;
    int r = lookup(path, &ino, &in);
    if (r != 0) return r;
    if (in.type == FT_CHARDEV) return -ENODEV;
    if (off >= MAX_FILE_SIZE) return -EFBIG;
    if (off + (off_t)size > MAX_FILE_SIZE) size = (size_t)(MAX_FILE_SIZE - off);   /* what fits */
    if ((uint32_t)off > in.size && (r = fill_zeros(ino, in.size, (uint32_t)off)) != 0) return r;
    r = file_write(ino, (uint32_t)off, buf, (uint32_t)size);
    if (r < 0) return host_err(r);
    return r == 0 && size > 0 ? -ENOSPC : r;
}

/* truncate: like sys_open's O_TRUNC (file_truncate), and longer by writing zeros */
static int lfs_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    (void)fi;
    uint32_t ino;
    struct inode in;
    int r = lookup(path, &ino, &in);
    if (r != 0) return r;
    if (in.type == FT_DIR) return -EISDIR;
    if (size > MAX_FILE_SIZE) return -EFBIG;
    if ((uint32_t)size > in.size) return fill_zeros(ino, in.size, (uint32_t)size);
    r = file_truncate(ino, (uint32_t)size);
    return r == FS_OK ? 0 : host_err(r);
}

/* unlink: like sys_unlink (fs_open_parent + file_delete) */
static int lfs_unlink(const char *path)
{
    uint32_t parent;
    const char *name;
    int r = lookup_parent(path, &parent, &name);
    if (r != 0) return r;
    r = file_delete(parent, name);
    return r == FS_OK ? 0 : host_err(r);
}

/* mkdir: like sys_mkdir (fs_open_parent + fs_mkdir) */
static int lfs_mkdir(const char *path, mode_t mode)
{
    (void)mode;
    uint32_t parent;
    const char *name;
    int r = lookup_parent(path, &parent, &name);
    if (r != 0) return r;
    r = fs_mkdir(parent, name);
    return r < 0 ? host_err(r) : 0;
}

/* rmdir: like sys_rmdir (fs_open_parent + fs_rmdir) */
static int lfs_rmdir(const char *path)
{
    uint32_t parent;
    const char *name;
    int r = lookup_parent(path, &parent, &name);
    if (r != 0) return r;
    r = fs_rmdir(parent, name);
    return r == FS_OK ? 0 : host_err(r);
}

/* link: like sys_link (fs_open + fs_open_parent + fs_link) */
static int lfs_link(const char *from, const char *to)
{
    uint32_t ino, parent;
    struct inode in;
    const char *name;
    int r = lookup(from, &ino, &in);
    if (r != 0) return r;
    if (in.type == FT_DIR) return -EPERM;        /* the host's answer; fs_link says FS_ERR_INVALID */
    r = lookup_parent(to, &parent, &name);
    if (r != 0) return r;
    r = fs_link(ino, parent, name);
    return r == FS_OK ? 0 : host_err(r);
}

/* rename: like sys_rename (fs_open_parent twice + fs_rename). On the host, renaming onto an existing
 * name replaces it (an editor saves a file that way); LogOS's rename refuses, so it goes first. */
static int lfs_rename(const char *from, const char *to, unsigned int flags)
{
    uint32_t old_parent, new_parent, from_ino, to_ino;
    const char *old_name, *new_name;
    struct inode from_in, to_in;
    size_t from_len = strlen(from);
    if (flags & 2) return -EINVAL;               /* exchange the two (Linux, macOS): LogOS cannot */
    int r = lookup_parent(from, &old_parent, &old_name);
    if (r != 0) return r;
    r = lookup_parent(to, &new_parent, &new_name);
    if (r != 0) return r;
    if ((r = lookup(from, &from_ino, &from_in)) != 0) return r;
    if (from_in.type == FT_DIR && strncmp(to, from, from_len) == 0 && to[from_len] == '/') {
        return -EINVAL;                          /* a directory into itself */
    }
    if (lookup(to, &to_ino, &to_in) == 0) {
        if (to_ino == from_ino) return 0;
        if (flags != 0) return -EEXIST;          /* the host asked not to replace */
        if (from_in.type == FT_DIR && to_in.type != FT_DIR) return -ENOTDIR;
        if (from_in.type != FT_DIR && to_in.type == FT_DIR) return -EISDIR;
        r = to_in.type == FT_DIR ? fs_rmdir(new_parent, new_name) : file_delete(new_parent, new_name);
        if (r != FS_OK) return host_err(r);
    }
    r = fs_rename(old_parent, old_name, new_parent, new_name);
    return r == FS_OK ? 0 : host_err(r);
}

/* statfs: the superblock's counts */
static int lfs_statfs(const char *path, struct statvfs *st)
{
    (void)path;
    memset(st, 0, sizeof(*st));
    st->f_bsize = st->f_frsize = BLOCK_SIZE;
    st->f_blocks = sb.total_blocks;
    st->f_bfree = st->f_bavail = sb.free_blocks;
    st->f_files = sb.total_inodes;
    st->f_ffree = st->f_favail = sb.free_inodes;
    st->f_namemax = MAX_FILENAME - 1;
    return 0;
}

/* LogOS has no permissions, owners or times: accept and forget them, so cp -p or touch work */
static int lfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void)mode; (void)fi;
    uint32_t ino;
    return lookup(path, &ino, NULL);
}

static int lfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
    (void)uid; (void)gid; (void)fi;
    uint32_t ino;
    return lookup(path, &ino, NULL);
}

static int lfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    (void)tv; (void)fi;
    uint32_t ino;
    return lookup(path, &ino, NULL);
}

/* LogOS has no extended attributes either: the host's are dropped, and none are there. Refusing
 * them would make macOS keep them in "._" files of their own, a LogOS inode each. */
#ifndef ENOATTR
#define ENOATTR ENODATA
#endif
static int lfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    (void)name; (void)value; (void)size; (void)flags;
    uint32_t ino;
    return lookup(path, &ino, NULL);
}

static int lfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    (void)path; (void)name; (void)value; (void)size;
    return -ENOATTR;
}

static int lfs_listxattr(const char *path, char *list, size_t size)
{
    (void)list; (void)size;
    uint32_t ino;
    return lookup(path, &ino, NULL);             /* 0: an empty list */
}

static int lfs_removexattr(const char *path, const char *name)
{
    (void)path; (void)name;
    return -ENOATTR;
}

static void *lfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void)conn;
    cfg->use_ino = 1;                            /* hard links show as one file (ls -i) */
    cfg->entry_timeout = 0;                      /* nothing cached: a link count or size is always */
    cfg->attr_timeout = 0;                       /* LogOS's of the moment */
    cfg->negative_timeout = 0;
    return NULL;
}

static const struct fuse_operations lfs_ops = {
    .init     = lfs_init,
    .getattr  = lfs_getattr,
    .readdir  = lfs_readdir,
    .open     = lfs_open,
    .create   = lfs_create,
    .mknod    = lfs_mknod,
    .read     = lfs_read,
    .write    = lfs_write,
    .truncate = lfs_truncate,
    .unlink   = lfs_unlink,
    .mkdir    = lfs_mkdir,
    .rmdir    = lfs_rmdir,
    .link     = lfs_link,
    .rename   = lfs_rename,
    .statfs   = lfs_statfs,
    .chmod    = lfs_chmod,
    .chown    = lfs_chown,
    .utimens  = lfs_utimens,
    .setxattr = lfs_setxattr,
    .getxattr = lfs_getxattr,
    .listxattr = lfs_listxattr,
    .removexattr = lfs_removexattr,
};

int main(int argc, char *argv[])
{
    struct stat st;
    if (argc < 3) {
        fprintf(stderr, "usage: %s block_storage.bin mountpoint [FUSE options]\n"
                        "  e.g. on macOS with macFUSE: -o backend=fskit\n", argv[0]);
        return 1;
    }
    if (stat(argv[1], &st) != 0 || native_backing_store_open(argv[1]) != 0) {
        fprintf(stderr, "%s: cannot open %s\n", argv[0], argv[1]);
        return 1;
    }
    int r = fs_mount();
    if (r == FS_ERR_VERSION) {
        fprintf(stderr, "%s: %s is a LogOS disk of file system version %u; this logosfs reads "
                        "version %u\n", argv[0], argv[1], sb.version, FS_VERSION);
        return 1;
    }
    if (r != FS_OK) {
        fprintf(stderr, "%s: %s is not a LogOS disk image\n", argv[0], argv[1]);
        return 1;
    }
    image_time = st.st_mtime;

    /* FUSE's arguments: everything but the image, single-threaded, named after this driver */
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, argv[0]);
    for (int i = 2; i < argc; i++) fuse_opt_add_arg(&args, argv[i]);
    fuse_opt_add_arg(&args, "-s");
    fuse_opt_add_arg(&args, "-ofsname=logosfs");
    r = fuse_main(args.argc, args.argv, &lfs_ops, NULL);
    fuse_opt_free_args(&args);
    native_backing_store_close();
    return r;
}
