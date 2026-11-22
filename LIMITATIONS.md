# What LogOS Belfield does not do, on purpose

LogOS is the teaching operating system of the textbook *Operating Systems from the Bare Metal Up*.
Belfield is its first version: one program runs at a time. The book reads this code line by line,
and its exercises ask the reader to explain, measure or repair the shortcomings listed here.

**Do not repair them in this repository.** Each one is kept because an exercise depends on it.
A change that removes one of them takes the exercise away from every reader of the book. Bugs
that are not on this list are bugs: please report them.

Each entry says what the behaviour is, where it is in the code, and which chapter or exercise of
the book uses it. In the code each place carries a one-line comment that begins `LIMITATIONS.md`
and gives the entry's number; this command finds them all:

    grep -rn 'LIMITATIONS.md' kernel user boot fstool

## The file system

**B5. Nothing is cached: every read goes to the device.**
`file_read` (`kernel/file.c`) reads the file's i-node and then the data block from the block
device on every call, and `inode_read` (`kernel/inode.c`) reads the i-node's block every time it
is asked. Reading a file one byte at a time costs two block transfers for each byte.
*Book:* Chapter 7, Exercise 7.4; Chapter 11, Section "One command, from the keyboard to the
screen" and Exercise 11.8, which adds a one-block cache.

**B8. The block allocator tests one bit at a time.**
`block_alloc` (`kernel/block.c`) reads the bitmap block once and then looks at its bits one by
one from the start of the data area. `bitmap_get`, which nothing calls, reads the bitmap block
from the device on every call.
*Book:* Chapter 7, Exercise 7.7, which makes the search skip whole bytes.

**B9. One buffer for the whole file system.**
`block_buf` (`kernel/fs.c`) is the only block buffer, and `block.c`, `inode.c`, `dir.c` and
`file.c` all use it. A function must take what it needs out of the buffer before it calls
another function that fills it; the order of the calls in `dir_add` and in `file_write` matters
for that reason.
*Book:* Chapter 7, Exercise 7.8.

**B10. The size of a directory only grows.**
`dir_remove` (`kernel/dir.c`) leaves the size alone when it frees a slot, and `dir_add` adds 32
to it even when it fills a freed slot. The size is therefore an upper bound on the slots to
search, not a count of the entries. (Release v1.0.1 made the size smaller in `dir_remove`, and
lost the last entry of the directory: that is the bug the comment in `dir_remove` records.)
*Book:* Chapter 7, Section "Walk-through: directories" and Exercise 7.6.

**B11. `fstool` cannot remove or link.**
`fstool` (`fstool/fstool.c`) has four commands: `format`, `ls`, `mkdir` and `add`.
*Book:* Chapter 7, Exercise 7.9, which adds `rm` and `ln`.

**B16. The bootloader reads direct blocks only.**
The loop that loads the kernel (`boot/boot_main.c`) stops after the 60 direct blocks of the
i-node, which is every block a Belfield file can have.
*Book:* Chapter 11, Exercise 11.2.

## System calls

**B1. `open` ignores the access mode.**
`sys_open` (`kernel/syscall.c`) looks at `O_CREAT`, `O_TRUNC` and `O_APPEND` and at nothing else
in its flags, and `struct fd_entry` (`kernel/syscall.h`) has no place to remember how a file was
opened. A program can write to a file that it opened with `O_RDONLY`.
*Book:* Chapter 8, Exercise 8.9, which records the mode and checks it in `read` and `write`.

**B2. A negative exit status looks like a program that could not be loaded.**
`spawn` has one return value for two things: the exit status of the child, which `sys_exit`
(`kernel/syscall.c`) puts into the parent's `a0`, and a negative error code when the program
cannot be loaded. The shell (`run_external` in `user/shell.c`) takes every negative value for
"not here, try the next directory of `PATH`", so after `exit(-1)` it reports `command not found`
for a command that it has just run. This is also why a program that is stopped by an exception
ends with status 128 plus the cause, and not with -1 (`c_trap_handler` in `kernel/main.c`).
*Book:* Chapter 8, Exercise 8.10; Chapter 9, Exercise 9.7.

**B12. Twenty system calls, numbered 0 to 19, and no `getpid`.**
`kernel/syscall_nr.h` is the whole list. Process identifiers come from a counter that only goes
up (`proc_alloc` in `kernel/process.c`).
*Book:* Chapter 8, Exercise 8.8, which adds `getpid` as number 20. Do not add, remove or
renumber a system call.

**B15. `spawn` starts a program and waits for it.**
`sys_spawn` (`kernel/syscall.c`) switches to the child at once and marks the parent
`PROC_READY`, which in Belfield means "suspended until its child exits". It never returns to the
dispatcher; `sys_exit` of the child finishes the parent's call: it stores the status in the
parent's `a0` and moves the parent's saved program counter past the `ecall`. There is no
background execution, and the child gets fresh descriptors on the console
(`proc_fd_init`).
*Book:* Chapter 8, Exercise 8.7; Chapter 9, Exercise 9.10; Chapter 11, Exercises 11.9 and 11.10.

## The shell

**B3. The shell has no comments, no `;`, no quotes and no redirection.**
`main` in `user/shell.c` expands variables in the whole line first (`expand_vars`) and splits it
into words afterwards (`parse_cmd`), so a value that contains a blank becomes two words.
`#`, `;`, `>`, `<`, `|`, `"`, `'` and `\` are ordinary characters. A line has at most seven
words: `parse_cmd` stops when it has started the seventh, so the seventh word runs to the end of
the line, blanks and all.
*Book:* Chapter 9, Section "Walk-through: what the Belfield shell does not do" and Exercises
9.1, 9.6, 9.8 (comments and `;`), 9.9 (quotes) and 9.10 (redirection).

## Devices and the console

**B4. There is one device driver, the console, and no null device.**
`main` (`kernel/main.c`) registers the console driver as major number 1 and nothing else, so a
device file with another major number can be created with `mknod` and cannot be read or written.
*Book:* Chapter 11, Exercise 11.4 (`/dev/null` and `/dev/zero`) and Exercise 11.6 (a driver of
the reader's own).

**B6. The console driver delivers raw keys.**
`console_read` (`kernel/console_dev.c`) waits in a loop for each key and returns only when it
has as many bytes as it was asked for. It does not echo, it has no line editing, and it never
reports the end of the input. `gets` (`user/libc.c`) does the echo for the shell.
*Book:* Chapter 11, Section "Walk-through: the console driver" and Exercise 11.5.

**B7. The kernel prints without the driver.**
The kernel's `printf` (`kernel/console.c`) stores each character in the terminal's register
directly, so `main` can print the name and the version before it calls `console_dev_init`.
*Book:* Chapter 11, Exercise 11.3, which removes the call and asks what still appears.

## Memory and protection

**B13. Every program runs in machine mode.**
The kernel never leaves machine mode (`c_trap_handler` in `kernel/main.c` tests for cause 11, an
`ecall` from machine mode), and a program can read and write the kernel, another slot or a
device register. Nothing stops a program's stack from growing down into its own code.
*Book:* Chapter 8, Exercise 8.1; Chapter 11, Section "What Belfield cannot do".

**B14. Nothing is relocated when a program is loaded.**
`elf_load_at` (`kernel/loader.c`) copies each segment of a program that was linked for address 0
to its place in a slot and moves the entry point by the same distance. No address stored inside
the program is changed. The programs work because they are compiled as
position-independent code (`-fPIE -fno-jump-tables` in the `Makefile`); a pointer stored in
initialised data is wrong after loading, which is why the user programs fill their arrays of
pointers when they run.
*Book:* Chapter 11, Section "Position-independent code" and Exercise 11.7.
