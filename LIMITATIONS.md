# What LogOS Glendalough does not do, on purpose

LogOS is the teaching operating system of the textbook *Operating Systems from the Bare Metal Up*.
Glendalough is its third version: Blackrock with virtual memory. The kernel runs in supervisor
mode and each program in user mode, in an address space of its own. The book reads this code line
by line, and its exercises ask the reader to explain, measure or repair the shortcomings listed
here.

**Do not repair them in this repository.** Each one is kept because an exercise depends on it.
A change that removes one of them takes the exercise away from every reader of the book. Bugs
that are not on this list are bugs: please report them.

Each entry says what the behaviour is, where it is in the code, and which chapter or exercise of
the book uses it. In the code each place carries a one-line comment that begins `LIMITATIONS.md`
and gives the entry's number; this command finds them all:

    grep -rn 'LIMITATIONS.md' kernel user boot fstool

Entries G1 to G7 are Glendalough's own. Entries with a K are kept from Blackrock and entries with
a B from Belfield; the `LIMITATIONS.md` of those branches describes them in full.

## Virtual memory

**G1. A program can overwrite its own code.**
`build_user_mappings` (`kernel/vm.c`) maps the whole slot of a process with one set of
permissions, `PTE_USER_RWX` (`kernel/vm.h`): readable, writable and executable. The linker
script `user/user.lds` puts code and data into one segment.
*Book:* Chapter 21, Section "What Glendalough cannot do" and Exercise 21.4.

**G2. There is no guard page under the stack.**
The stack starts 256 bytes below the end of the slot and grows down towards the program's data,
and every page between them is mapped. A stack that grows too far overwrites data and causes no
fault.
*Book:* Chapter 21, Exercise 21.5.

**G3. The kernel translates a program's pointers by arithmetic, and accepts a shared segment that
is not attached.**
`uva_to_pa` (`kernel/vm.c`) does not read the page table. It accepts any address in the program's
slot and any address in the window of the eight shared-memory segments, whether or not the
process has attached the segment; `uva_region_end` has the same test. Through a system call, a
process can read and write a segment that its own page table does not map.
*Book:* Chapter 21, Section "Translating user pointers in system calls", the Try-it of Section
"Shared memory through page tables", and Exercise 21.6.

**G5. `fork` still copies the whole slot, and a page fault is always fatal.**
`sys_fork` (`kernel/syscall.c`) copies every byte of the parent's slot before the child runs.
There is no allocator of pages: every process has a slot of fixed size. `c_trap_handler`
(`kernel/main.c`) ends a process that causes a page fault; no fault is ever repaired.
*Book:* Chapter 21, Section "What Glendalough cannot do" and Exercise 21.9.

**G6. The page-table pool has 32 pages, and three of them are never returned.**
`PT_POOL_PAGES` (`kernel/vm.h`) is the size of the pool. The two shared level-0 tables and the
root table that `main` builds before the first process exists (`kernel_pt_root` in
`kernel/main.c`) stay allocated for ever; each process takes two more.
*Book:* Chapter 21, Section "The page-table pool" and Exercises 21.1 and 21.3.

**G7. The mapping functions still know megapages, and Glendalough creates none.**
`map_page`, `unmap_page` and `pt_free_all` (`kernel/vm.c`) test for a level-1 entry that is a
leaf. An earlier version mapped the kernel with 4 MB pages; the tests remain.
*Book:* Chapter 21, Section "Mapping pages" and Exercise 21.1.

## Users

**G4. There are no users and no permissions.**
An i-node (`struct inode` in `kernel/fs_types.h`) has no owner and no mode, a process
(`struct process` in `kernel/process.h`) has no user, and no system call checks who is asking:
every process may open every file and kill every process. `sys_open` (`kernel/syscall.c`)
ignores the access mode in its flags (entry B1 of Belfield), so a file opened for reading can be
written.
*Book:* Chapter 23, Section "Walk-through: LogOS has no users" and Exercises 23.8, 23.9 and
23.10.

## Kept from Blackrock

| Entry | Behaviour | Where in Glendalough | Book |
|---|---|---|---|
| K1 | an orphan's exit is taken for the shell's, and an orphan cannot be killed either | `sys_exit` and `sys_kill`, `kernel/syscall.c`; `s_mode_main`, `kernel/main.c` | Exercise 15.4 |
| K2 | no idle process: `schedule` returns when nothing is ready | `schedule`, `kernel/process.c` | Exercises 15.5, 16.2, 16.4 and 17.8 |
| K3 | the console and the disk are polled with interrupts off | `console_read`, `kernel/console_dev.c`; `kernel/block.c` | Exercises 15.6, 17.7 and 17.8 |
| K4 | no `sleep` system call; `SLEEP_TIMER` is unused | `kernel/process.h` | Exercise 15.7 |
| K5 | `kill` wakes a parent asleep in `spawn` without finishing its call | `sys_kill`, `kernel/syscall.c` | Exercise 15.8 |
| K7 | a pipe wakes every process that sleeps on it | `pipe_wakeup`, `kernel/pipe.c` | Exercises 16.5 and 16.8 |
| K8 | a semaphore wakes every sleeper; nobody holds a semaphore | `kernel/sem.c` | Exercises 19.7 and 19.9 |
| K9 | pipes have no names | | Exercise 16.10 |
| K10 | one quantum for every process; slots scanned in order | `kernel/timer.h`; `schedule`, `kernel/process.c` | Exercises 15.3 and 17.3 to 17.7 |
| K11 | a shared segment never attached is never freed | `shm_get`, `kernel/shm.c` | Exercise 19.8 |
| K12 | a zombie keeps its slot, its shared memory, its semaphores and its page tables | `sys_exit`, `kernel/syscall.c` | Exercise 16.9 |
| K13 | `cat` ignores what `write` returns | `user/cat.c` | Exercise 16.6 |
| K14 | a write to a pipe with no reader returns -1, and that is all | `pipe_write`, `kernel/pipe.c` | Exercise 16.7 |
| K16 | the kernel never enables interrupts for itself | `kernel/trap.S`; `s_mode_main`, `kernel/main.c` | Chapter 21, Section "What Glendalough cannot do" |
| K17 | `wait` returns the exit status, not the process identifier | `sys_wait`, `kernel/syscall.c` | Exercises 15.2 and 15.8 |

## Kept from Belfield

| Entry | Behaviour | Where in Glendalough | Book |
|---|---|---|---|
| B1 | `open` ignores the access mode | `sys_open`, `kernel/syscall.c` | Exercises 8.9 and 23.9 |
| B4 | the console is the only device driver | `main`, `kernel/main.c` | Exercises 11.4 and 11.6 |
| B5 | nothing is cached: every read goes to the device | `file_read`, `kernel/file.c` | Exercises 7.4 and 11.8 |
| B8 | the block allocator tests one bit at a time | `block_alloc`, `kernel/block.c` | Exercise 7.7 |
| B9 | one buffer for the whole file system | `block_buf`, `kernel/fs.c` | Exercise 7.8 |
| B10 | the size of a directory only grows | `dir_add`, `dir_remove`, `kernel/dir.c` | Exercise 7.6 |
| B11 | `fstool` cannot remove or link | `fstool/fstool.c` | Exercise 7.9 |
