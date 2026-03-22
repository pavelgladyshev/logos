# logOS

A bare-metal Unix-like operating system for RISC-V, designed to run on a simulated RISC-V computer in [Logisim Evolution](https://github.com/pavelgladyshev/logisim-evolution-ucd).

logOS implements a three-stage boot process (bootloader, kernel, shell), an inode-based filesystem, a character device driver framework, position-independent executable loading, per-process environment variables, and a system call interface modeled on Unix conventions.

## Quick Start

### Prerequisites

- **Java** (for Logisim Evolution)
- **RISC-V cross-compiler**: `riscv64-elf-gcc` and `riscv64-elf-ld`
  - macOS (Homebrew): `brew install riscv-gnu-toolchain`
  - Linux: `sudo apt install build-essential binutils-riscv64-unknown-elf gcc-riscv64-unknown-elf`
- **Logisim Evolution UCD** v4.0.6+: [download from releases](https://github.com/pavelgladyshev/logisim-evolution-ucd/releases)

Update the `RISCV_TOOL_PREFIX` variable in the Makefile if your toolchain is in a different location (default: `/opt/homebrew/bin/riscv64-elf-`).

### Build

```bash
make all
```

This builds:
- `boot-rom.txt` — Bootloader ROM image
- `kernel.bin` — Kernel binary
- User programs (`hello`, `ls`, `cat`, `mkdir`, `rmdir`, `mknod`, `env_demo`, `spawn_demo`)
- `block_storage.bin` — Filesystem image containing the kernel and all user programs

### Run

1. **First time**: Open `computer.circ` in Logisim Evolution GUI, load `boot-rom.txt` into the ROM component and `block_storage.bin` into the block device, then save the circuit.

2. **Run from terminal** (faster, no GUI overhead):
   ```bash
   java -jar logisim-evolution-ucd-4.0.6-all.jar computer.circ --tty raw
   ```

You should see the boot sequence followed by an interactive shell prompt:

```
Bootloader starting...
BOOT: Filesystem found
BOOT: Jumping to kernel at 0x100000

Welcome to logOS Belfield 1.0!
Mounting filesystem...
Filesystem mounted.
Starting shell

/$ _
```

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
                              | (32KB)            |
                              +-------------------+ 0x00118000
                              | Process Slot 1    | <- spawned programs
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

- 8 fixed 32KB memory slots, each holding one program
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
