# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A bare-metal Unix-like OS for RISC-V, designed to run on a simulated RISC-V computer (Logisim). Uses a three-stage boot process (bootloader → kernel → shell): a ROM bootloader loads the kernel from the filesystem into RAM, the kernel initializes subsystems and loads the shell, and the shell provides an interactive command-line interface. Implements block-based storage, inode-based file management (max 32 inodes), directory support with path traversal, a character device driver framework, position-independent executable loading, preemptive multitasking with fork/exec/wait, kernel-buffered pipes for IPC, and a system call interface for user programs.

## Build Commands

```bash
make                      # Build bootloader ROM image (boot-rom.txt)
make all                  # Build everything: boot-rom.txt, kernel.bin, user programs, filesystem image
make fstool               # Build native fstool utility for host system
make fs-image             # Create filesystem image (block_storage.bin)
make user-programs        # Build all user programs
make clean                # Clean object files and executables
make clean-all            # Clean everything including user programs
make check-pie            # Check user programs for PIE compatibility issues
```

### fstool Usage

```bash
./fstool format <blockfile> <num_blocks>    # Format new filesystem
./fstool ls <blockfile> <path>              # List directory contents
./fstool mkdir <blockfile> <path>           # Create directory
./fstool add <blockfile> <path> <hostfile>  # Copy host file into filesystem
```

## Running

```bash
# Logisim (requires Java and Logisim Evolution)
java -jar ~/logisim-evolution-ucd-4.0.6-all.jar computer.circ -t tty

# QEMU (alternative - compiles for RV64)
make qemu                 # Run in QEMU with serial console
make qemu-gdb             # Run in QEMU, wait for GDB at localhost:1234
```

Note: Before running Logisim without GUI, you must first load the bootloader ROM image (`boot-rom.txt`) into the ROM component and the filesystem image (`block_storage.bin`) into the block device via the GUI, then save the circuit.

## Architecture

Key modules organized by directory:

| Layer | Files | Purpose |
|-------|-------|---------|
| **Bootloader** (`boot/`) | | |
| Boot Entry | `boot_crt0.S`, `boot.lds` | CPU reset entry point, bootloader linker script |
| Boot Main | `boot_main.c` | Loads kernel binary from filesystem into RAM |
| Boot I/O | `boot_io.c` | Block device DMA read for bootloader |
| Boot FS | `boot_fs.c`, `boot_string.c` | Minimal read-only filesystem traversal |
| **Kernel** (`kernel/`) | | |
| Block I/O | `block.c/h` | Memory-mapped block device access, bitmap operations |
| Inode | `inode.c/h` | Read/write inodes, allocate/free, metadata queries |
| Directory | `dir.c/h` | Directory entry lookup, add/remove entries, mkdir/rmdir |
| File | `file.c/h` | Create/delete files, read/write data, truncate |
| Filesystem | `fs.c/h` | Format, mount, open paths (absolute), superblock access |
| Device | `device.c/h`, `console_dev.c/h` | Character device driver framework, console driver |
| Trap Handling | `trap.S`, `trap.h` | Assembly trap handler, context save/restore |
| Process Mgmt | `process.c/h` | Process table, slot allocation, per-process state |
| Pipes | `pipe.c/h` | Kernel-buffered IPC pipes with reference counting |
| System Calls | `syscall.c/h`, `syscall_nr.h` | Syscall dispatch, per-process file descriptors |
| ELF Loader | `loader.c/h`, `loader_asm.S`, `elf.h` | Load ELF programs at arbitrary addresses |
| Kernel Entry | `crt0.S`, `kernel.lds` | Kernel entry point (in RAM), kernel linker script |
| Kernel Main | `main.c` | Kernel initialization and shell restart loop |
| **User** (`user/`) | | |
| User Runtime | `user_crt0.S`, `user.lds` | User program startup, linker script |
| User Library | `libc.c/h`, `syscall.S` | User-space libc with syscall wrappers |
| Test Helpers | `test_helpers.h` | Shared pass/fail/results helpers for test programs |
| Shell | `shell.c` | Interactive shell (`/bin/sh`) |
| **Host Tools** | | |
| Host Tool | `fstool.c`, `native_block.c` | Native utility to manage filesystem images |

**Filesystem Layout (512 blocks × 512 bytes):**
- Block 0: Superblock
- Blocks 1+: Block allocation bitmap
- Subsequent blocks: Inode table (32 inodes × 128 bytes)
- Remaining: Data blocks

**Memory Layout (Logisim):**
- 0x00000000-0x0000FFFF: ROM (64KB) — bootloader code (`boot_crt0.S` + `boot_main.c`)
- 0x00100000-0x0010FFFF: RAM — kernel code/data/BSS (loaded by bootloader from `/boot/kernel`)
- 0x00110000-0x0014FFFF: Process memory slots (8 × 32KB)
  - Slot 0: 0x00110000-0x00117FFF (shell)
  - Slot 1: 0x00118000-0x0011FFFF
  - ...
  - Slot 7: 0x00148000-0x0014FFFF
- 0x001F0000-0x001FEFFF: Bootloader BSS (60KB, used only during boot)
- 0x001FFFFC: Initial stack pointer (shared by bootloader and kernel)
- 0x00200000: Block device MMIO
- 0xFFFF0004/0008: Console input MMIO (receiver control/data)
- 0xFFFF000C: Console output MMIO

Each 32KB slot holds code + data + BSS + stack (stack grows down from slot top - 0x100).

## Boot Process

The system uses a three-stage boot: **Bootloader → Kernel → Shell**.

### Stage 1: Bootloader (ROM)

On CPU reset, execution starts at address `0x00000000` in ROM (`boot/boot_crt0.S`):

1. Set stack pointer to `0x001FFFFC` (top of RAM)
2. Zero bootloader BSS at `0x001F0000` (placed high to avoid collision with kernel)
3. Call `boot_main()` which:
   - Reads the superblock (block 0) from the block device, validates filesystem magic
   - Resolves path `/boot/kernel` by walking the directory tree from root inode
   - DMA-copies the kernel binary's data blocks directly into RAM at `0x00100000`
   - Jumps to `0x00100000` via function pointer (never returns)

The bootloader has its own minimal filesystem implementation (`boot_fs.c`, `boot_io.c`) — just enough to read files. Its BSS lives at `0x001F0000` so the kernel load at `0x00100000` doesn't overlap.

### Stage 2: Kernel Initialization (RAM)

The kernel entry point (`kernel/crt0.S`) runs at `0x00100000`:

1. Set stack pointer to `0x001FFFFC`, zero kernel BSS
2. Call `main()` which performs initialization in this order:
   - `console_dev_init()` — register console character device driver (major 1)
   - `proc_init()` — initialize 8 process slots with fixed memory addresses
   - `fs_mount()` — read superblock, make filesystem accessible
3. Enter shell restart loop (runs forever):
   - Allocate process slot 0, init environment (`PATH=/bin`, `CWD=/`)
   - `elf_load_at("/bin/sh", 0x110000, ...)` — load shell ELF into slot 0
   - Set up trap frame (entry point, stack, fds 0/1/2 → console)
   - Install trap handler via `mtvec`/`mscratch` CSRs
   - `run_user_program()` — save kernel context, `mret` to shell's `_start`
   - *(blocks here until shell calls `sys_exit`)*
   - Print exit code, free slot, loop back to reload shell

### Stage 3: Shell

The shell's `_start` (`user/user_crt0.S`) calls `main()` in `user/shell.c`, which enters an interactive command loop displaying `CWD$ ` prompt.

When the shell exits (user types `exit`):
- `sys_exit` detects no parent process (`parent == -1`)
- Assembly restores kernel context saved by `run_user_program()`
- `main()` resumes, prints `"[ exit() code X, reloading shell... ]"`, and restarts the shell

### Build Artifacts

- `boot-rom.txt` — Bootloader ROM image (loaded into Logisim ROM component)
- `kernel.bin` — Flat kernel binary (stored at `/boot/kernel` in the filesystem image)
- `block_storage.bin` — Filesystem image (loaded into Logisim block device) containing `/boot/kernel`, `/bin/sh`, and other user programs

## System Call Interface

User programs communicate with the kernel via the `ecall` instruction. The syscall ABI follows RISC-V conventions:
- **a7**: Syscall number
- **a0-a5**: Arguments
- **a0**: Return value (negative = error)

| Number | Name | Args | Description |
|--------|------|------|-------------|
| 0 | sys_exit | a0=status | Terminate program |
| 1 | sys_read | a0=fd, a1=buf, a2=len | Read from file descriptor |
| 2 | sys_write | a0=fd, a1=buf, a2=len | Write to file descriptor |
| 3 | sys_open | a0=path, a1=flags | Open file, return fd; flags: O_RDONLY(0), O_WRONLY(1), O_CREAT(0x100), O_TRUNC(0x200), O_APPEND(0x400) |
| 4 | sys_close | a0=fd | Close file descriptor |
| 5 | sys_spawn | a0=path, a1=argv, a2=envp | Launch child in new slot with inherited env; returns child's exit code (envp overrides child's env if non-NULL) |
| 6 | sys_readdir | a0=path, a1=buf, a2=max | List directory entries |
| 7 | sys_mkdir | a0=path | Create a directory |
| 8 | sys_rmdir | a0=path | Remove an empty directory |
| 9 | sys_mknod | a0=path, a1=major, a2=minor | Create a device node |
| 10 | sys_setenv | a0=name, a1=value | Set environment variable in current process |
| 11 | sys_getenv | a0=name, a1=buf, a2=buflen | Get environment variable from current process |
| 12 | sys_unsetenv | a0=name | Remove environment variable from current process |
| 13 | sys_getenv_count | (none) | Return number of env variables in current process |
| 14 | sys_getenv_entry | a0=index, a1=buf, a2=buflen | Get Nth env entry as "KEY=value" from current process |
| 15 | sys_chdir | a0=path | Change current working directory |
| 16 | sys_fork | (none) | Fork current process; returns child PID to parent, 0 to child, -1 on error |
| 17 | sys_exec | a0=path, a1=argv, a2=envp | Replace process image; returns -1 on failure (never returns on success) |
| 18 | sys_wait | (none) | Wait for any child to exit; returns child's exit code, -1 if no children |
| 19 | sys_getpid | (none) | Return PID of current process |
| 20 | sys_dup | a0=oldfd | Duplicate fd; returns lowest available fd, -1 on error |
| 21 | sys_dup2 | a0=oldfd, a1=newfd | Duplicate fd to specific number; returns newfd, -1 on error |
| 22 | sys_pipe | a0=pipefd[2] | Create pipe; pipefd[0]=read end, pipefd[1]=write end; returns 0/-1 |
| 23 | sys_unlink | a0=path | Delete a file (decrements link count, frees inode when 0) |
| 24 | sys_link | a0=target, a1=linkpath | Create hard link (new directory entry for existing inode) |
| 25 | sys_rename | a0=oldpath, a1=newpath | Move/rename file or directory |
| 26 | sys_stat | a0=path, a1=statbuf | Get file metadata (inode, size, type, link_count) |
| 27 | sys_kill | a0=pid | Terminate a process by PID |
| 28 | sys_ps | a0=buf, a1=maxprocs | List active processes into struct proc_info array |

**Pre-opened file descriptors:**
- fd 0: stdin (console device)
- fd 1: stdout (console device)
- fd 2: stderr (console device)

## Process Model

Each program runs in its own fixed 32KB memory slot with a per-process trap frame, file descriptors, and PID. Preemptive multitasking via timer interrupt (round-robin scheduling).

**Process states:**
- `PROC_FREE` (0) — slot available
- `PROC_RUNNING` (1) — currently executing on CPU
- `PROC_READY` (2) — runnable, waiting for CPU
- `PROC_SLEEPING` (3) — blocked (waiting for child, I/O, etc.)
- `PROC_ZOMBIE` (4) — exited, waiting for parent to collect exit code

**Sleep reasons** (for PROC_SLEEPING):
- `SLEEP_NONE` (0) — not sleeping
- `SLEEP_CHILD` (1) — parent waiting in spawn()
- `SLEEP_WAIT` (2) — parent waiting in wait()
- `SLEEP_TIMER` (3) — reserved for future use
- `SLEEP_IO` (4) — blocked on pipe read/write

**fork() semantics:** `fork()` copies the parent's entire 32KB memory slot to a new slot, duplicates all file descriptors (incrementing pipe reference counts), copies the environment, and adjusts the child's registers by the memory offset delta. Returns child PID to parent, 0 to child.

**exec() semantics:** `exec()` replaces the current process image with a new program. File descriptors are preserved across exec (important for pipe redirection via fork+dup2+exec). Environment is preserved unless envp is non-NULL.

**wait() semantics:** `wait()` blocks the calling process (SLEEP_WAIT) until any child exits. When a child calls exit(), it becomes a zombie, wakes the parent, and the parent collects the exit code.

**spawn() semantics:** `spawn()` combines fork+exec+wait in a single syscall (legacy). Loads the child into a new slot, parent sleeps until child exits.

**Pipes:** Kernel-buffered IPC channels (256-byte circular buffer, max 8 pipes). Created via `pipe()`, integrated with `read()/write()/close()/dup()/dup2()/fork()`. Blocking uses ecall-retry: when a pipe operation would block, the process sleeps without advancing mepc past the ecall instruction; when woken, the ecall re-executes and retries the operation. Reference counting tracks open read/write ends independently. EOF is returned when all write ends are closed; EPIPE when all read ends are closed.

**Context switching:** The scheduler uses round-robin with timer interrupts. `sys_fork`, `sys_exec`, `sys_exit`, and blocking pipe operations call `schedule()` which selects the next PROC_READY process and switches to it via `trap_ret()`.

**Shell uses fork/exec/wait:** The shell forks a child process, the child calls exec() to run the command, and the parent calls wait() to collect the exit code. This is standard Unix process creation.

**Environment:** Per-process, stored in `struct process`. Each process has its own `env[MAX_ENVC][MAX_ENV_LEN]` array. On `fork()` or `spawn()`, the child inherits a copy of the parent's environment. Changes to env in a child do not affect the parent (Unix semantics). CWD is also per-process.

## Writing User Programs

User programs use the user-space library (`user/libc.h`) which provides syscall wrappers:

```c
#include "libc.h"

int main(void) {
    puts("Hello from user program!");  /* puts() appends newline (standard C) */
    printf("main() is at address 0x%x\n", (unsigned int)main);
    return 42;  // Exit code returned via sys_exit
}
```

The user library provides:
- **I/O functions**: `putchar()`, `puts()`, `printf()`, `getchar()`, `gets()`, `read()`, `write()`
- **File operations**: `open()`, `close()`, `dup()`, `dup2()`, `pipe()`, `unlink()`, `link()`, `rename()`, `stat()`
- **Directory operations**: `readdir()`, `mkdir()`, `rmdir()`, `mknod()`
- **Process control**: `exit()`, `fork()`, `exec()`, `execve()`, `wait()`, `getpid()`, `spawn()`, `spawnve()`, `chdir()`, `kill()`, `ps()`
- **Environment**: `setenv()`, `unsetenv()`, `env_count()`, `getenv()`, `getenv_r()`, `getenv_entry()`, `env_to_envp()`
- **String functions**: `strlen()`, `strcmp()`, `strncmp()`, `strcpy()`, `strncpy()`, `strchr()`, `atoi()`
- **Memory functions**: `memset()`, `memcpy()`, `memcmp()`

Build produces user programs that are added to the filesystem image:
- `/bin/hello` — Hello world demo
- `/bin/spawn_demo` — Demonstrates `spawn()` syscall
- `/bin/sh` — Interactive shell (loaded by kernel after boot, uses fork/exec/wait)
- `/bin/ls` — List directory contents
- `/bin/mkdir` — Create directories
- `/bin/rmdir` — Remove empty directories
- `/bin/mknod` — Create device nodes
- `/bin/env_demo` — Demonstrates environment variable API
- `/bin/cat` — Display file contents (supports stdin and file arguments; useful as pipeline component)
- `/bin/fork_demo` — Demonstrates fork(), exec(), wait(), getpid()
- `/bin/pipe_demo` — Demonstrates pipe(), fork(), dup2() for IPC
- `/bin/pipe_test` — Comprehensive pipe edge-case tests (partial I/O, EPIPE, buffer capacity, dup refcounting)
- `/bin/redir_test` — Tests pipe redirection with fork+dup2+exec (capture stdout, feed stdin, pipelines)
- `/bin/fd_test` — File descriptor management tests (close/reuse, dup/dup2 edge cases, fd inheritance)
- `/bin/rm` — Remove files
- `/bin/mv` — Move/rename files and directories
- `/bin/ln` — Create hard links
- `/bin/cp` — Copy files
- `/bin/ps` — List active processes (PID, state, parent)
- `/bin/kill` — Terminate a process by PID
- `/bin/ed` — Minimal line editor (open, print, insert, append, delete, write, quit)

## Interactive Shell

After the bootloader loads the kernel and the kernel initializes, it loads `/bin/sh` from the filesystem into process slot 0. Available commands:

```bash
$ help                  # Show available commands
$ echo hello            # Print arguments
$ echo $PATH            # Variable expansion in arguments
$ echo $?               # Last program's exit code
$ set FOO=bar           # Set environment variable
$ set                   # List all environment variables
$ unset FOO             # Remove environment variable
$ cd /bin               # Change current directory
$ cd                    # Change to root directory
$ hello                 # Run /bin/hello (found via PATH)
$ ls                    # List current directory
$ ls /bin               # List /bin directory
$ mkdir /tmp            # Create /tmp directory
$ rmdir /tmp            # Remove /tmp directory
$ mknod /dev/tty 1 0    # Create device node (major 1, minor 0)
$ /bin/hello            # Run with full path
$ exit                  # Exit shell (kernel restarts it)
$ exit 42               # Exit with specific code

# I/O Redirection
$ echo hello > /tmp/test.txt     # Write stdout to file (creates/truncates)
$ echo more >> /tmp/test.txt     # Append stdout to file
$ cat < /tmp/test.txt            # Read stdin from file
$ cat < /tmp/in.txt > /tmp/out.txt  # Combined input + output redirection

# Pipes
$ echo hello | cat               # Pipe stdout of left to stdin of right
$ ls /bin | cat                   # Pipe directory listing through cat
$ echo hello | cat | cat          # Multi-stage pipeline (up to 4 stages)
```

The shell supports:
- Command line editing with backspace
- Current working directory (`cd`, CWD shown in prompt)
- Relative paths in all file operations (resolved against CWD)
- Environment variables (`set`, `unset`, `$VAR`, `${VAR}`, and `$?` expansion)
- Per-process environment: child programs inherit a copy, changes don't affect parent (Unix semantics)
- PATH-based command lookup (default: `PATH=/bin`)
- Running external programs via `fork()`+`exec()`+`wait()` (Unix-style process creation)
- I/O redirection: `>` (truncate), `>>` (append), `<` (input) — operators must be space-separated
- Pipes: `cmd1 | cmd2 [| cmd3 ...]` — multi-stage pipeline (up to 4 stages) connecting stdout to stdin via kernel pipes
- Built-in `echo` supports redirection (save/redirect/restore pattern); other built-ins do not
- Exit status tracking (`$?` holds last program's exit code)
- Automatic shell restart after `exit` command

**Important for position-independent code:**
- Use `-fno-jump-tables` to prevent switch statements from generating absolute address jump tables
- Use `-fPIE -mcmodel=medany` for PC-relative addressing
- **Avoid static pointer array initialization** - see "Known PIE Limitations" section below
- Run `make check-pie` or `./check_pie.sh <file.o>` to verify PIE compatibility

## Toolchain Requirements

RISC-V cross-compiler configured at `/opt/homebrew/bin/riscv64-elf-*` (macOS). For Linux:
```bash
sudo apt install build-essential binutils-riscv64-unknown-elf gcc-riscv64-unknown-elf
```

Update the `RISCV_TOOL_PREFIX` variable in the Makefile if your toolchain is in a different location.

## Target Configuration

- **Logisim**: RV32IM_Zicsr, ILP32 ABI
  - Bootloader: `boot/boot.lds` (ROM at 0x0, BSS at 0x001F0000)
  - Kernel: `kernel/kernel.lds` (loaded at 0x00100000)
  - User programs: `user/user.lds` (linked at address 0, relocated at load time)
- The `zicsr` extension is required for CSR instructions used in trap handling

## Known PIE Limitations

When writing position-independent user programs:
- **Avoid static pointer array initialization** — pointers get absolute addresses baked in
- **Avoid `static` or global arrays whose addresses are passed to syscalls** — the compiler may generate absolute `li` instructions instead of PC-relative `auipc` for their addresses

```c
/* BAD - pointers stored in .data use absolute addresses, won't be relocated */
char *argv[] = { "hello", "arg1", "arg2", NULL };

/* GOOD - build array at runtime using PC-relative string references */
char *argv[4];
argv[0] = "hello";
argv[1] = "arg1";
argv[2] = "arg2";
argv[3] = NULL;

/* BAD - static array address computed as absolute value by compiler */
static struct dirent entries[32];
readdir("/", entries, 32);  /* kernel receives wrong address */

/* GOOD - stack array address computed relative to sp */
struct dirent entries[32];
readdir("/", entries, 32);  /* kernel receives correct stack address */
```

### Technical Details

**Why static initialization fails:**

When GCC compiles a static pointer array like `char *argv[] = {"hello", ...}`, it:
1. Places string literals in `.rodata` (e.g., `.LC0`, `.LC1`)
2. Creates a `.data.rel.local` section containing pointers to those strings
3. Emits `R_RISCV_32` relocations for each pointer entry in the object file

Example relocation table in the object file:
```
Relocation section '.rela.data.rel.local':
 Offset     Type            Sym. Name
00000000  R_RISCV_32        .LC0       <- Pointer to "hello"
00000004  R_RISCV_32        .LC1       <- Pointer to "arg1"
00000008  R_RISCV_32        .LC2       <- Pointer to "arg2"
```

**The linker problem:** The bare-metal RISC-V linker (`riscv64-elf-ld`) doesn't support `-pie` or `-shared` flags needed for dynamic executables. It resolves all `R_RISCV_32` relocations at link time, embedding absolute addresses directly into `.data`:

```
Contents of section .data (after linking at address 0):
 0030 18000000 20000000 28000000 00000000
      ^        ^        ^
      0x18     0x20     0x28  (absolute link-time addresses)
```

When loaded at 0x110000, these pointers still contain 0x18, 0x20, 0x28 - completely wrong addresses. The ELF loader has relocation support, but the linker doesn't preserve relocations in the output.

**Why runtime initialization works:**

When you write `argv[0] = "hello"` in a function, GCC generates:
```asm
auipc  a5, %pcrel_hi(.LC0)    # PC + high bits of offset
addi   a5, a5, %pcrel_lo(.LC0) # + low bits = actual address
sw     a5, 0(sp)               # Store computed address
```

The `auipc` instruction computes addresses relative to the current PC, which is correct regardless of where the code is loaded. No runtime relocations needed - the address calculation happens in code.

### What Would Fix This

The bare-metal toolchain (`riscv64-elf-*`) intentionally disables PIE/shared library support. This is controlled by `ld/emulparams/elf32lriscv-defs.sh` in binutils:

```sh
# Enable shared library support for everything except an embedded elf target.
case "$target" in
  riscv*-elf)
    ;;  # Empty - PIE disabled for embedded targets
  *)
    GENERATE_SHLIB_SCRIPT=yes
    GENERATE_PIE_SCRIPT=yes
    ;;
esac
```

**Options to fix:**

1. **Patch binutils** - Remove the case statement and unconditionally enable:
   ```sh
   GENERATE_SHLIB_SCRIPT=yes
   GENERATE_PIE_SCRIPT=yes
   ```
   Then rebuild the toolchain. This is a [known workaround](https://groups.google.com/a/groups.riscv.org/g/sw-dev/c/NNyQB69-Owc) discussed by RISC-V developers.

2. **Use Linux toolchain** - `riscv64-unknown-linux-gnu-*` supports `-pie` but isn't designed for bare metal.

3. **Continue using runtime initialization** - Current workaround, avoids the toolchain limitation entirely.

The reason PIE is disabled: newlib doesn't have shared library support, and enabling it causes [binutils testsuite failures](https://lists.gnu.org/archive/html/bug-binutils/2020-02/msg00141.html). The GOT/PLT code generation works correctly - it's just administratively disabled.
