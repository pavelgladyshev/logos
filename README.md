# logOS

A bare-metal Unix-like operating system for RISC-V, designed to run on a simulated RISC-V computer in [Logisim Evolution](https://github.com/pavelgladyshev/logisim-evolution-ucd).

## Versions

| Version | Branch | Target | Key Features |
|---------|--------|--------|--------------|
| **1.0 Belfield** | [`v1.x`](../../tree/v1.x) | RV32IM M-mode (no timer required) | Single-tasking, spawn-based process model, filesystem, shell, basic utilities |
| **2.0 Blackrock** | [`main`](../../tree/main) | RV32IM_Zicsr M-mode + timer | Preemptive multitasking, fork/exec/wait, pipes, multi-stage pipelines, I/O redirection |
| *3.0 Glendalough* | *planned* | RV32 + SV32 MMU | Virtual memory, user/supervisor separation |

## Quick Start

### Setting up

LogOS builds and runs on macOS, on Linux (Ubuntu, Debian), and on Windows in Ubuntu on WSL 2.
`setup.sh` installs what is missing (the RISC-V cross-compiler, Java and FUSE), fetches Logisim
Evolution UCD, builds LogOS and checks that it boots:

```bash
git clone -b main https://github.com/pavelgladyshev/logos.git
cd logos
./setup.sh
```

- **macOS**: install [Homebrew](https://brew.sh) first.
- **Windows**: install Ubuntu on WSL 2 first: `wsl --install -d Ubuntu-24.04` in PowerShell, restart
  Windows, and open Ubuntu from the Start menu, which asks for a Linux user name and password. Then
  clone LogOS and run `setup.sh` in the Ubuntu window. Keep the clone in Ubuntu's own folders
  (`~/logos`), not under `/mnt/c`; Windows programs find it at
  `\\wsl.localhost\Ubuntu-24.04\home\<you>\logos`. Like a virtual machine, WSL 2 needs
  virtualization switched on in the computer's firmware (BIOS/UEFI).

`./setup.sh --check` only checks. When something is not right, the summary at the end says what:
paste all of the output when you ask for help. What it installs, to do it by hand:

- macOS: `brew install riscv64-elf-gcc`, and Java 21 or later (`brew install --cask temurin@21`).
- Ubuntu, Debian, WSL: `sudo apt install build-essential git curl pkg-config fuse3 libfuse3-dev
  gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf openjdk-21-jre`.
- Logisim Evolution UCD 5.0.6 or later: `make logisim` fetches its jar from the
  [releases](https://github.com/pavelgladyshev/logisim-evolution-ucd/releases).

The Makefile finds the RISC-V compiler itself (Homebrew's or Ubuntu's); another one:
`make RISCV_TOOL_PREFIX=/its/path/riscv64-elf-`.

### Build

```bash
make all
```

This builds:
- `boot-rom.txt` — Bootloader ROM image
- `kernel.bin` — Kernel binary
- User programs (shell, ls, cat, cp, rm, mv, ln, ed, ps, kill, mkdir, rmdir, mknod, and demos)
- `block_storage.bin` — Filesystem image containing the kernel and all user programs

`make all` (or `make fs-image`) makes a fresh `block_storage.bin`: files added to the old one are gone.

### Run

```bash
make run
```

runs LogOS in Logisim's text mode (`java -jar logisim-evolution-ucd-5.0.6-all.jar computer.circ --tty raw`):
the terminal is the computer's console, and the simulation runs much faster than in Logisim's
window. Ctrl-C stops it. (`make run` makes `block_storage.bin` only when there is none.) You should
see the bootloader load the kernel, and the shell's prompt:

```
Bootloader starting...
BOOT: Filesystem found
BOOT: Found kernel at inode 4
BOOT: Jumping to kernel at 0x100000

Welcome to logOS 2.0 Blackrock!

Mounting filesystem...
Filesystem mounted.

Starting shell
Type 'help' for available commands

Welcome to logOS!
/$ _
```

`make gui` opens `computer.circ` in Logisim's window. On Windows the window comes through WSLg,
with plainer graphics and more slowly; with Java for Windows installed and on the PATH (for
example [Temurin 21](https://adoptium.net)), `make gui` uses it instead, for a native window and
the FPGA board's USB.

`computer.circ` holds the bootloader in its ROM. After changing the bootloader (`boot/`), load
`boot-rom.txt` into the ROM in Logisim's window and save the circuit.

## Shell

```bash
/$ help                         # Show available commands
/$ echo hello                   # Print arguments
/$ echo $PATH                   # Variable expansion
/$ echo $?                      # Last program's exit code
/$ set FOO=bar                  # Set environment variable
/$ unset FOO                    # Remove environment variable
/$ cd /bin                      # Change current directory
/$ ls /bin                      # List directory

# File operations
/$ cat /etc/hello.txt           # Display file contents
/$ cp src.txt dst.txt           # Copy file
/$ rm file.txt                  # Remove file
/$ mv old.txt new.txt           # Move/rename file
/$ ln target.txt link.txt       # Create hard link
/$ ed file.txt                  # Edit file (line editor)

# Process management
/$ ps                           # List active processes
/$ kill 3                       # Terminate process by PID

# I/O redirection
/$ echo hello > /tmp/out.txt    # Write stdout to file
/$ echo more >> /tmp/out.txt    # Append to file
/$ cat < /tmp/out.txt           # Read stdin from file

# Pipes
/$ ls /bin | cat                # Single pipe
/$ ls /bin | cat | cat          # Multi-stage pipeline (up to 4 stages)
```

## Architecture

```
ROM (64KB)                    RAM
+-------------------+        +-------------------+ 0x00100000
| Bootloader        |------->| Kernel            |
| (boot_crt0.S +    | loads  | (code/data/BSS)   |
|  boot_main.c)     |        +-------------------+ 0x0010FFFF
+-------------------+        +-------------------+ 0x00110000
  0x00000000                 | Process Slot 0    | <- shell
                              | (32KB)            |
                              +-------------------+ 0x00118000
                              | Process Slot 1    |
                              | (32KB)            |
                              +-------------------+
                              | ...               |
                              +-------------------+ 0x0014FFFF
                              | Process Slot 7    |
                              +-------------------+

Block Device (MMIO @ 0x200000)     Console (MMIO @ 0xFFFF000C)
```

### Boot Process

1. **Bootloader** (ROM) — reads superblock, locates `/boot/kernel` in the filesystem, DMA-copies it to RAM at `0x100000`, jumps to kernel
2. **Kernel** (RAM) — initializes console, process table, and filesystem; enters a loop that loads and runs `/bin/sh`
3. **Shell** — interactive command loop with fork/exec/wait; when it exits, the kernel restarts it

### Filesystem

Inode-based filesystem (1024 blocks x 512 bytes):
- Block 0: Superblock (magic number, layout version, block/inode counts)
- Blocks 1+: Block allocation bitmap
- Subsequent blocks: Inode table (64 inodes x 128 bytes each)
- Remaining blocks: Data

The superblock's version (`FS_VERSION` in `kernel/fs_types.h`, 2 for this layout) changes
whenever the layout does. The kernel, `fstool` and `logosfs` all refuse a disk of another
version, so a disk image made by an older LogOS has to be made again: `make fs-image`.

Managed on the host with `fstool`:
```bash
./fstool/fstool format block_storage.bin 1024
./fstool/fstool mkdir block_storage.bin /mydir
./fstool/fstool add block_storage.bin /mydir/file hostfile.txt
./fstool/fstool ls block_storage.bin /
```

### Mounting a disk image on your computer (logosfs)

`fstool/logosfs` mounts a disk image as a folder of your computer, so that `ls`, `cp`, an
editor or the Finder work on LogOS's files. It is a file system driver of your computer's
kernel, through FUSE (File system in USErspace): the kernel passes the file operations on the
folder to logosfs, and logosfs answers them with LogOS's own file system code (`kernel/fs.c`,
`dir.c`, `file.c`, `inode.c`, `block.c`, built for the host as for `fstool`). Each of its
operations calls what the LogOS system call of the same job calls, so a change to LogOS's file
system is in logosfs too after `make logosfs`.

It needs FUSE:
- **macOS**: install [macFUSE](https://macfuse.github.io) 5, run
  `/Library/Filesystems/macfuse.fs/Contents/Resources/macfuse.app/Contents/MacOS/macfuse install --components file-system-extensions`
  in a terminal, and switch macFUSE on in System Settings → General → Login Items & Extensions →
  File System Extensions (ⓘ). No kernel extension is needed: logosfs runs through Apple's FSKit
  (`-o backend=fskit`).
- **Linux** (Debian, Ubuntu): `sudo apt install fuse3 libfuse3-dev pkg-config` (`setup.sh` does).
- **Windows**: as on Linux, in Ubuntu on WSL 2 (WSL 1 has no FUSE). For File Explorer to see the
  mounted folder, see below.

`setup.sh` builds logosfs when FUSE is there.

```bash
make logosfs
mkdir mnt
./fstool/logosfs block_storage.bin mnt -o backend=fskit    # on Linux without -o backend=fskit
ls -l mnt/bin
cp notes.txt mnt/etc/
umount mnt                                                  # on Linux: fusermount3 -u mnt
```

Unmount before booting the image in Logisim or writing it to an SD card. LogOS keeps no owners,
permissions, times or extended attributes: logosfs shows your user, the image file's time and fixed
permissions, and drops changes to them. Names have at most 27 characters, and a file at most
30,720 bytes (60 direct blocks).

On macOS with FSKit, `ln` makes the hard link but reports an I/O error all the same (macFUSE's
FSKit backend wants a hard link to come back as the original's node, which FUSE's path-based
interface does not do), and `ls -l` may show the first name's old link count. Writes are cached
there too: a full disk or a file too large shows as an I/O error when the file is closed.

On Windows, File Explorer finds the folder at `\\wsl.localhost\Ubuntu-24.04\home\<you>\logos\mnt`
(the distribution's name as `wsl -l` shows it). It reaches WSL's files through WSL's file server,
which runs as root, and FUSE shows a mounted folder to no other user, root included, unless it is
mounted with `-o allow_other`; an ordinary user may ask for that once `/etc/fuse.conf` allows it
(`setup.sh` does that on WSL):

```bash
sudo sed -i 's/^#user_allow_other/user_allow_other/' /etc/fuse.conf
./fstool/logosfs block_storage.bin mnt -o allow_other
```

Windows puts LogOS's refusals in its own words: a file stops at 161,280 bytes with "A device
attached to the system is not functioning", and a name longer than 27 characters has "incorrect
syntax". Windows programs may end lines with CR LF, where LogOS's programs expect LF alone.

### System Calls

Programs use `ecall` with the syscall number in `a7` and arguments in `a0`-`a5`:

| # | Name | Description |
|---|------|-------------|
| 0 | exit | Terminate program |
| 1 | read | Read from file descriptor |
| 2 | write | Write to file descriptor |
| 3 | open | Open file, return fd |
| 4 | close | Close file descriptor |
| 5 | spawn | Run child program (legacy, combines fork+exec+wait) |
| 6 | readdir | List directory entries |
| 7 | mkdir | Create directory |
| 8 | rmdir | Remove empty directory |
| 9 | mknod | Create device node |
| 10 | setenv | Set environment variable |
| 11 | getenv | Get environment variable |
| 12 | unsetenv | Remove environment variable |
| 13 | getenv_count | Count environment variables |
| 14 | getenv_entry | Get Nth env entry |
| 15 | chdir | Change working directory |
| 16 | fork | Fork process (returns child PID / 0) |
| 17 | exec | Replace process image |
| 18 | wait | Wait for child to exit |
| 19 | getpid | Get current PID |
| 20 | dup | Duplicate file descriptor |
| 21 | dup2 | Duplicate fd to specific number |
| 22 | pipe | Create pipe pair |
| 23 | unlink | Delete file |
| 24 | link | Create hard link |
| 25 | rename | Move/rename file or directory |
| 26 | stat | Get file metadata |
| 27 | kill | Terminate process by PID |
| 28 | ps | List active processes |

### Process Model

- 8 fixed 32KB memory slots, each holding one program
- Preemptive multitasking with timer-driven round-robin scheduling
- `fork()` copies the parent's 32KB slot to a new slot, duplicates file descriptors and environment
- `exec()` replaces the process image, preserving file descriptors (important for pipe redirection)
- `wait()` blocks until a child exits and returns its exit code
- Kernel-buffered pipes (256-byte circular buffer, max 8 pipes) with blocking I/O and reference counting
- Per-process file descriptors, environment variables, and working directory
- Process states: FREE, READY, RUNNING, SLEEPING, ZOMBIE

## Writing User Programs

```c
#include "libc.h"

int main(void) {
    puts("Hello from logOS!");
    printf("main() is at address 0x%x\n", (unsigned int)main);
    return 0;
}
```

The user library (`user/libc.h`) provides:
- **I/O**: `putchar()`, `puts()`, `printf()`, `getchar()`, `gets()`, `read()`, `write()`
- **Files**: `open()`, `close()`, `dup()`, `dup2()`, `pipe()`, `unlink()`, `link()`, `rename()`, `stat()`
- **Directories**: `readdir()`, `mkdir()`, `rmdir()`, `mknod()`
- **Process**: `exit()`, `fork()`, `exec()`, `execve()`, `wait()`, `getpid()`, `spawn()`, `kill()`, `ps()`
- **Environment**: `setenv()`, `unsetenv()`, `getenv()`, `chdir()`, `env_count()`, `getenv_entry()`
- **Strings**: `strlen()`, `strcmp()`, `strncmp()`, `strcpy()`, `strncpy()`, `strchr()`
- **Memory**: `memset()`, `memcpy()`, `memcmp()`

Programs are compiled as position-independent executables (PIE) and loaded at arbitrary addresses. See [CLAUDE.md](CLAUDE.md) for PIE limitations and workarounds.

## Source Layout

| Directory | Contents |
|-----------|----------|
| `boot/` | Bootloader — ROM entry point, minimal FS reader, block device DMA |
| `kernel/` | Kernel — filesystem, process management, syscalls, trap handling, ELF loader, pipes |
| `user/` | User space — libc, shell, and utility programs |
| `fstool/` | Host tools: `fstool` builds filesystem images, `logosfs` mounts them (FUSE) |
| `textbook/` | Course materials |

## License

Licensed under [Creative Commons Attribution International License 4.0](https://creativecommons.org/licenses/by/4.0/).
