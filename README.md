# logOS

A bare-metal Unix-like operating system for RISC-V, designed to run on a simulated RISC-V computer in [Logisim Evolution](https://github.com/pavelgladyshev/logisim-evolution-ucd).

logOS implements a three-stage boot process (bootloader, kernel, shell), an inode-based filesystem, a character device driver framework, position-independent executable loading, per-process environment variables, and a system call interface modeled on Unix conventions.

## Quick Start

### Setting up

LogOS builds and runs on macOS, on Linux (Ubuntu, Debian), and on Windows in Ubuntu on WSL 2.
`setup.sh` installs what is missing (the RISC-V cross-compiler, Java and FUSE), fetches Logisim
Evolution UCD, builds LogOS and checks that it boots:

```bash
git clone -b v1.x_belfield https://github.com/pavelgladyshev/logos.git
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
- Logisim Evolution UCD 5.0.5 or later: `make logisim` fetches its jar from the
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
- User programs (`hello`, `ls`, `cat`, `mkdir`, `rmdir`, `mknod`, `env_demo`, `spawn_demo`)
- `block_storage.bin` — Filesystem image containing the kernel and all user programs

`make all` (or `make fs-image`) makes a fresh `block_storage.bin`: files added to the old one are gone.

### Run

```bash
make run
```

runs LogOS in Logisim's text mode (`java -jar logisim-evolution-ucd-5.0.5-all.jar computer.circ --tty raw`):
the terminal is the computer's console, and the simulation runs much faster than in Logisim's
window. Ctrl-C stops it. (`make run` makes `block_storage.bin` only when there is none.) You should
see the bootloader load the kernel, and the shell's prompt:

```
Bootloader starting...
BOOT: Filesystem found
BOOT: Found kernel at inode 4
BOOT: Jumping to kernel at 0x100000

Welcome to logOS Belfield 1.0!

Mounting filesystem...
Filesystem mounted.

Starting shell
Type 'help' for available commands

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
/$ help                  # Show available commands
/$ echo hello            # Print arguments
/$ echo $PATH            # Variable expansion
/$ echo $?               # Last program's exit code
/$ set FOO=bar           # Set environment variable
/$ set                   # List all environment variables
/$ unset FOO             # Remove environment variable
/$ cd /bin               # Change current directory
/$ ls                    # List current directory
/$ ls /bin               # List /bin directory
/$ hello                 # Run /bin/hello (found via PATH)
/$ cat                   # Read /etc/hello.txt byte by byte
/$ mkdir /tmp            # Create directory
/$ rmdir /tmp            # Remove empty directory
/$ exit                  # Exit shell (kernel restarts it)
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
                              | (64KB)            |
                              +-------------------+ 0x00120000
                              | Process Slot 1    | <- spawned programs
                              | (64KB)            |
                              +-------------------+
                              | ...               |
                              +-------------------+ 0x0018FFFF
                              | Process Slot 7    |
                              +-------------------+

Block Device (MMIO @ 0x200000)     Console (MMIO @ 0xFFFF000C)
```

### Boot Process

1. **Bootloader** (ROM) — reads superblock, locates `/boot/kernel` in the filesystem, DMA-copies it to RAM at `0x100000`, jumps to kernel
2. **Kernel** (RAM) — initializes console, process table, and filesystem; enters a loop that loads and runs `/bin/sh`
3. **Shell** — interactive command loop; when it exits, the kernel restarts it

### Filesystem

Inode-based filesystem (256 blocks x 512 bytes):
- Block 0: Superblock (magic number, block/inode counts)
- Blocks 1+: Block allocation bitmap
- Subsequent blocks: Inode table (32 inodes x 128 bytes each)
- Remaining blocks: Data

Managed on the host with `fstool`:
```bash
./fstool/fstool format block_storage.bin 256
./fstool/fstool mkdir block_storage.bin /mydir
./fstool/fstool add block_storage.bin /mydir/file hostfile.txt
./fstool/fstool ls block_storage.bin /
```

### System Calls

Programs use `ecall` with the syscall number in `a7` and arguments in `a0`-`a5`:

| # | Name | Description |
|---|------|-------------|
| 0 | exit | Terminate program |
| 1 | read | Read from file descriptor |
| 2 | write | Write to file descriptor |
| 3 | open | Open file, return fd |
| 4 | close | Close file descriptor |
| 5 | spawn | Run child program, return its exit code |
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

### Process Model

- 8 fixed 64KB memory slots, each holding one program
- No preemptive multitasking — one process runs at a time
- `spawn()` suspends the parent, runs the child in a new slot, and resumes the parent with the child's exit code
- Per-process file descriptors, environment variables, and working directory
- Child inherits a copy of parent's environment (Unix semantics)

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
- **Files**: `open()`, `close()`
- **Directories**: `readdir()`, `mkdir()`, `rmdir()`, `mknod()`
- **Programs**: `exit()`, `spawn()`, `spawnve()`, `chdir()`
- **Environment**: `setenv()`, `unsetenv()`, `getenv()`, `env_count()`, `getenv_entry()`, `env_to_envp()`
- **Strings**: `strlen()`, `strcmp()`, `strncmp()`, `strcpy()`, `strncpy()`, `strchr()`
- **Memory**: `memset()`, `memcpy()`, `memcmp()`

Programs are compiled as position-independent executables (PIE) and loaded at arbitrary addresses. See [CLAUDE.md](CLAUDE.md) for PIE limitations and workarounds.

## Source Layout

| Directory | Contents |
|-----------|----------|
| `boot/` | Bootloader — ROM entry point, minimal FS reader, block device DMA |
| `kernel/` | Kernel — filesystem, process management, syscalls, trap handling, ELF loader |
| `user/` | User space — libc, shell, and utility programs |
| `fstool/` | Host tool for building filesystem images |
| `textbook/` | Course materials |

## License

Licensed under [Creative Commons Attribution International License 4.0](https://creativecommons.org/licenses/by/4.0/).
