# What LogOS Blackrock does not do, on purpose

LogOS is the teaching operating system of the textbook *Operating Systems from the Bare Metal Up*.
Blackrock is its second version: processes, `fork` and `exec`, pipes, shared memory, semaphores
and a timer. The book reads this code line by line, and its exercises ask the reader to explain,
measure or repair the shortcomings listed here.

**Do not repair them in this repository.** Each one is kept because an exercise depends on it.
A change that removes one of them takes the exercise away from every reader of the book. Bugs
that are not on this list are bugs: please report them.

Each entry says what the behaviour is, where it is in the code, and which chapter or exercise of
the book uses it. In the code each place carries a one-line comment that begins `LIMITATIONS.md`
and gives the entry's number; this command finds them all:

    grep -rn 'LIMITATIONS.md' kernel user boot fstool

Entries K1 to K18 are Blackrock's own. Entries with a B are kept from Belfield, whose own
`LIMITATIONS.md` describes them in full.

## Processes

**K1. An orphan's exit is taken for the shell's.**
When a process exits, `sys_exit` (`kernel/syscall.c`) gives its living children the parent -1,
the value that only the shell had. When such an orphan exits in its turn, `sys_exit` returns 1,
as it does for the shell, and `main` (`kernel/main.c`) frees the shell's slot and loads a new
shell while the old shell's children are still running. `sys_kill` refuses a victim whose
parent is -1, so an orphan cannot be killed either.
*Book:* Chapter 15, Section "fork, exec, wait and exit" and Exercise 15.4.

**K5. `kill` wakes a parent that is asleep in `spawn` without finishing its call.**
`sys_kill` (`kernel/syscall.c`) makes its victim a zombie with exit status -1 and makes a
sleeping parent ready, but leaves the parent's `a0` and saved program counter alone. A parent
asleep in `wait` runs `wait` again and collects the zombie; a parent asleep in `spawn` runs
`spawn` again and starts the program a second time. The status -1 is also what `wait` returns
when there are no children.
*Book:* Chapter 15, Section "Sleep and wakeup" and Exercise 15.8.

**K6. `fork` copies the whole slot and adjusts four registers.**
`sys_fork` (`kernel/syscall.c`) copies every byte of the parent's slot, one word at a time, and
adds the distance between the slots to the child's `sp` and program counter, and to `ra` and `s0`
if they point into the parent's slot. No pointer stored in memory is adjusted. The copy takes
longer than a quantum, which is why the child usually runs first.
*Book:* Chapter 15, Section "fork, exec, wait and exit" and Exercises 15.9 and 15.10.

**K12. A zombie keeps its slot, its shared memory and its semaphores.**
`sys_exit` closes the pipe ends of a process that becomes a zombie, so that readers see the end
of the file; everything else waits for `proc_free`, which runs when the parent calls `wait`.
*Book:* Chapter 15, Section "fork, exec, wait and exit"; Chapter 16, Exercise 16.9.

**K17. `wait` returns the exit status and not the process identifier.**
`sys_wait` (`kernel/syscall.c`) returns the status of the zombie it collects, and -1 when the
caller has no children, so a parent cannot tell which child ended.
*Book:* Chapter 15, Exercises 15.2 and 15.8.

## Scheduling

**K2. There is no idle process.**
`schedule` (`kernel/process.c`) switches to the next ready process. When no process is ready it
returns to its caller, and the system call that wanted to sleep returns instead: a `read` from an
empty pipe gives -2 to a process that is still marked as sleeping.
*Book:* Chapter 15, Section "Blocking system calls" and Exercise 15.5; Chapter 16, Exercises
16.2 and 16.4; Chapter 17, Exercise 17.8.

**K3. The console and the disk are polled with interrupts off.**
`console_read` (`kernel/console_dev.c`) waits for a key in a loop, inside a system call, where
interrupts are disabled; `block_read` and `block_write` (`kernel/block.c`) wait for the device in
the same way. While the shell waits at its prompt no other process runs.
*Book:* Chapter 15, Section "Blocking system calls" and Exercise 15.6; Chapter 17, Exercises
17.7 and 17.8; Chapter 18, Exercise 18.9.

**K4. There is no `sleep` system call.**
`SLEEP_TIMER` (`kernel/process.h`) is defined for it and nothing uses it; the comment of
`sleep_chan` already speaks of a wake-up time.
*Book:* Chapter 15, Exercise 15.7.

**K10. One quantum for every process, and the slots are scanned in order.**
`TIME_SLICE` (`kernel/timer.h`) is the quantum of every process. The timer branch of
`c_trap_handler` (`kernel/main.c`) sets the next alarm before it calls `schedule`, so the time
that `schedule` takes comes out of the next process's quantum. `schedule` takes the first ready
slot after the current one; there are no priorities and no queue.
*Book:* Chapter 15, Exercise 15.3; Chapter 17, Section "Walk-through: the Blackrock scheduler"
and Exercises 17.3 to 17.7.

**K16. The kernel never enables interrupts.**
Every trap disables interrupts, and nothing in the kernel enables them again before it returns
to a process: `enable_interrupts` and `disable_interrupts` (`kernel/trap.S`) are defined and
never called. The kernel needs no locks for that reason, and it has one trap stack for all
processes (`trap_stack` in `kernel/main.c`); the trap handler could not be interrupted safely.
*Book:* Chapter 18, Section "Walk-through: how Blackrock protects its own data" and Exercises
18.7 and 18.9.

**K15. `mstatus` is not part of a process's saved state.**
The trap frame (`kernel/trap.h`) holds the general registers and the program counter. A process
that disables interrupts and then sleeps hands its setting to the process that runs next.
*Book:* Chapter 18, Exercise 18.8.

**K18. External interrupts are enabled and nothing handles them.**
`timer_init` (`kernel/process.c`) enables the timer and the external interrupt in `mie`;
`c_trap_handler` reports any interrupt other than the timer's as unexpected.
*Book:* Chapter 15, Section "The timer interrupt and the round-robin scheduler"; Chapter 17,
Exercise 17.8, which uses the keyboard's interrupt.

## Pipes, shared memory and semaphores

**K7. A pipe wakes every process that sleeps on it.**
`pipe_wakeup` (`kernel/pipe.c`) makes every sleeper on the pipe ready, readers and writers alike;
each runs its system call again and may go back to sleep.
*Book:* Chapter 16, Section "Walk-through: pipes in the Blackrock kernel" and Exercises 16.5 and
16.8.

**K9. Pipes have no names.**
A pipe exists only as two descriptors made by `pipe`; no file type and no path leads to one.
*Book:* Chapter 16, Exercise 16.10.

**K13. `cat` ignores what `write` returns.**
`user/cat.c` copies in pieces of 64 bytes and does not look at the result of `write`.
*Book:* Chapter 16, Exercise 16.6.

**K14. Writing to a pipe that nobody reads returns -1, and that is all.**
`pipe_write` (`kernel/pipe.c`) returns -1 when the pipe has no reader. There are no signals, so
a writer that ignores the result carries on.
*Book:* Chapter 16, Exercise 16.7.

**K8. A semaphore wakes every process that sleeps on it, and nobody holds a semaphore.**
`sem_wakeup` (`kernel/sem.c`) makes every sleeper ready; `sem_post` and `sem_close` both call it.
The kernel keeps no record of which process has passed a `semwait`: `sys_semwait` and
`sys_sempost` accept any identifier from 0 to 7, and a process that ends between `semwait` and
`sempost` does not give its unit back (`proc_close_sems` in `kernel/process.c` closes the handle
and posts nothing).
*Book:* Chapter 19, Section "Walk-through: shared memory and semaphores in Blackrock" and
Exercises 19.7 and 19.9.

**K11. A shared segment that is created and never attached is never freed.**
`shm_get` (`kernel/shm.c`) starts a new segment with a reference count of 0; only `shmat` takes a
reference. `semget`, by contrast, takes one when it creates a semaphore.
*Book:* Chapter 19, Exercise 19.8.

## Kept from Belfield

| Entry | Behaviour | Where in Blackrock | Book |
|---|---|---|---|
| B1 | `open` ignores the access mode | `sys_open`, `kernel/syscall.c` | Exercise 8.9 |
| B4 | the console is the only device driver | `main`, `kernel/main.c` | Exercises 11.4 and 11.6 |
| B5 | nothing is cached: every read goes to the device | `file_read`, `kernel/file.c` | Exercises 7.4 and 11.8 |
| B7 | the kernel prints without the driver | `main`, `kernel/main.c`; `kernel/console.c` | Exercise 11.3 |
| B8 | the block allocator tests one bit at a time | `block_alloc`, `kernel/block.c` | Exercise 7.7 |
| B9 | one buffer for the whole file system | `block_buf`, `kernel/fs.c` | Exercise 7.8 |
| B10 | the size of a directory only grows | `dir_add`, `dir_remove`, `kernel/dir.c` | Exercise 7.6 |
| B11 | `fstool` cannot remove or link | `fstool/fstool.c` | Exercise 7.9 |
| B13 | every program runs in machine mode | `c_trap_handler`, `kernel/main.c` | Exercises 8.1 and 18.8; Chapter 23, Exercise 23.7 |
| B14 | nothing is relocated when a program is loaded | `elf_load_at`, `kernel/loader.c` | Exercise 11.7 |
