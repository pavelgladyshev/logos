# ESP32-C3 logOS Context

This file is a technical map of the ESP32-C3 logOS firmware in this repository. It is based on the current source tree, `logOS/sdkconfig`, `logOS/partitions.csv`, `logOS/main/CMakeLists.txt`, the local `logOS/components/esp_system` component, and the libraries present under `logOS/build/esp-idf`.

External ESP32-C3 context used by this reference:

- Espressif ESP-IDF Application Startup Flow: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/startup.html>
- Espressif ESP-IDF Bootloader guide: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/bootloader.html>
- Espressif ESP-IDF Partition Tables guide: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/partition-tables.html>
- Espressif ESP32-C3 datasheet: <https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf>

## Table of Contents

1. [Chapter 1: Platform and Build Context](#chapter-1-platform-and-build-context)
2. [Chapter 2: Boot and Startup Flow](#chapter-2-boot-and-startup-flow)
3. [Chapter 3: Application Flows](#chapter-3-application-flows)
4. [Chapter 4: Kernel Runtime](#chapter-4-kernel-runtime)
5. [Chapter 5: Storage and Filesystem](#chapter-5-storage-and-filesystem)
6. [Chapter 6: Processes, Syscalls, and Traps](#chapter-6-processes-syscalls-and-traps)
7. [Chapter 7: ELF Loading and User Execution](#chapter-7-elf-loading-and-user-execution)
8. [Chapter 8: Console and Devices](#chapter-8-console-and-devices)
9. [Chapter 9: Standalone Boot Filesystem Reader](#chapter-9-standalone-boot-filesystem-reader)
10. [Chapter 10: Local ESP-IDF Support](#chapter-10-local-esp-idf-support)
11. [Chapter 11: Built-In ESP-IDF API Appendix](#chapter-11-built-in-esp-idf-api-appendix)
12. [Chapter 12: Verification Notes](#chapter-12-verification-notes)

<a id="chapter-1-platform-and-build-context"></a>

## [Chapter 1: Platform and Build Context](#chapter-1-platform-and-build-context)

Chapter Purpose: Defines the target chip, flash layout, watchdog and console settings, and which parts of the repository own the bootloader, app, kernel, local ESP-IDF support, and ignored/generated files.

The configured target is ESP32-C3 (`CONFIG_IDF_TARGET="esp32c3"`), using the RISC-V ESP-IDF port (`CONFIG_IDF_TARGET_ARCH="riscv"`). The SoC is configured as a single-core target (`CONFIG_FREERTOS_UNICORE=y`) with the default CPU frequency set to 160 MHz (`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160`). Console output uses UART0 at 115200 baud (`CONFIG_ESP_CONSOLE_UART_DEFAULT=y`, `CONFIG_ESP_CONSOLE_UART_NUM=0`, `CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200`), with USB serial/JTAG enabled as the secondary console.

Flash configuration is DIO mode at 80 MHz and 2 MB (`CONFIG_ESPTOOLPY_FLASHMODE="dio"`, `CONFIG_ESPTOOLPY_FLASHFREQ="80m"`, `CONFIG_ESPTOOLPY_FLASHSIZE="2MB"`). The partition table offset is `0x8000`. `logOS/partitions.csv` defines:

| Name | Type | Subtype | Offset | Size | Use |
| --- | --- | --- | --- | --- | --- |
| `nvs` | `data` | `nvs` | `0x9000` | `0x6000` | ESP-IDF NVS |
| `phy_init` | `data` | `phy` | `0xf000` | `0x1000` | RF PHY data |
| `factory` | `app` | `factory` | `0x10000` | `1M` | main app image |
| `logosfs` | `data` | `0x40` | `0x110000` | `256K` | fixed logOS filesystem partition |

Watchdogs are enabled in configuration: bootloader WDT is enabled with a 9000 ms timeout, interrupt WDT is enabled with a 300 ms timeout, and task WDT is enabled with a 5 second timeout. The configured ESP-IDF panic mode is print-and-reboot.

`logOS/main/CMakeLists.txt` currently compiles:

- Application entry files: `logOS.c`, `logos_start.c`.
- Shell and user syscall shim: `components/shell/shell.c`, `components/user/user_syscall.c`.
- Kernel runtime: block, console, device, directory, file, filesystem allocation, filesystem core, inode, kernel time, loader, loader assembly, fixed partition access, process table, syscall dispatcher, trap C, and trap assembly.

Important distinction: `logOS/main/components/logos_cpu_start.c` and `logOS/main/components/kernel_start.c` exist in the tree, but they are not listed in `logOS/main/CMakeLists.txt` in the current build file. The compiled app still defines [kernel_start](#kernel_start) and [app_main](#app_main) in `logOS/main/logOS.c`; any direct boot into `kernel_start` depends on a startup object such as `logos_cpu_start.c` being linked by the build.

`logOS/bootloader_components/main` provides a custom second-stage bootloader override.

`logOS/components/esp_system` is a local copy/override of ESP-IDF's `esp_system` component. It is ESP-IDF-derived support code, not logOS kernel code, but it is part of this build and is documented separately below.

<a id="chapter-2-boot-and-startup-flow"></a>

## [Chapter 2: Boot and Startup Flow](#chapter-2-boot-and-startup-flow)

Chapter Purpose: Documents the reset-to-kernel path, including the custom second-stage bootloader, optional custom application startup, and the compiled fallback entry points.

After reset, the ESP32-C3 ROM bootloader runs from mask ROM. Espressif documents that ROM code determines the boot mode from strap pins and reset reason, configures SPI flash, loads the second-stage bootloader from flash offset `0x0`, and jumps to the second-stage bootloader entry point.

The second-stage bootloader reads the partition table, selects an app partition, loads app segments into RAM or maps flash-backed segments, verifies the image, and jumps to the app entry point. In a normal ESP-IDF app, that entry point is `call_start_cpu0` in `esp_system/port/cpu_start.c`; it performs port-level CRT/hardware initialization, calls `start_cpu0`, runs system initialization, starts FreeRTOS, and eventually calls `app_main` from the main task.

logOS changes the normal shape in two places:

- The custom bootloader under `logOS/bootloader_components/main` changes second-stage behavior and partition selection.
- The custom startup code in `logOS/main/components/logos_cpu_start.c`, when linked, bypasses the normal ESP-IDF `SYS_STARTUP_FN()` component-init walk and calls [kernel_start](#kernel_start) directly.

### Custom Second-Stage Bootloader

<a id="call_start_cpu0"></a>
<a id="bootloader-call_start_cpu0"></a>

### Bootloader call_start_cpu0

Definition: `logOS/bootloader_components/main/bootloader_start.c:23`.

Purpose: bootloader entry point for CPU0. It sets up bootloader state, initializes bootloader hardware/services through [bootloader_init](#bootloader_init), selects an application partition with [select_partition_number](#select_partition_number), loads the selected image with ESP-IDF bootloader support, and jumps to the loaded image.

Inputs: none from C callers; entered by ROM/bootloader handoff.

Outputs: does not return on success. On fatal failure it resets or halts through ESP-IDF bootloader support paths.

Side effects: initializes early bootloader hardware, reads the partition table, loads an app image, prints a configured welcome message through `esp_rom_printf`, and transfers execution to the selected app.

Important callees: [bootloader_init](#bootloader_init), [select_partition_number](#select_partition_number), ESP-IDF `bootloader_utility_*` functions.

### select_partition_number

Definition: `logOS/bootloader_components/main/bootloader_start.c:53`.

Purpose: selects the boot partition index from `bootloader_state_t`.

Inputs: pointer to ESP-IDF bootloader state.

Outputs: integer partition index.

Side effects: none beyond reading bootloader state.

Important callers: [Bootloader call_start_cpu0](#bootloader-call_start_cpu0).

### __getreent

Definition: `logOS/bootloader_components/main/bootloader_start.c:67`.

Purpose: supplies a minimal Newlib reentrancy pointer required by linked bootloader code.

Inputs: none.

Outputs: pointer to a static `_reent`.

Side effects: exposes one static reentrancy object for bootloader use.

### bootloader_enable_cpu_reset_info

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:51`.

Purpose: ESP32-C3 bootloader hook for enabling CPU reset information. The local implementation is minimal.

Inputs: none.

Outputs: none.

Side effects: none in the local implementation.

### bootloader_dump_wdt_reset_info

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:58`.

Purpose: ESP32-C3 bootloader hook for printing WDT reset diagnostics.

Inputs: CPU index.

Outputs: none.

Side effects: may print reset diagnostics depending on local code/configuration.

### bootloader_check_if_wdt_reset

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:65`.

Purpose: classifies whether a CPU reset reason is a watchdog reset.

Inputs: CPU index and `soc_reset_reason_t`.

Outputs: boolean.

Side effects: none.

### bootloader_super_wdt_auto_feed

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:76`.

Purpose: local helper for bootloader super-WDT handling.

Inputs: none.

Outputs: none.

Side effects: touches WDT-related hardware registers when enabled by target support.

### bootloader_hardware_init

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:84`.

Purpose: performs low-level ESP32-C3 bootloader hardware initialization.

Inputs: none.

Outputs: none.

Side effects: initializes bootloader hardware state before image loading.

### bootloader_ana_reset_config

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:94`.

Purpose: configures analog reset behavior for ESP32-C3 boot.

Inputs: none.

Outputs: none.

Side effects: writes reset/analog control registers.

### bootloader_init

Definition: `logOS/bootloader_components/main/bootloader_esp32c3.c:125`.

Purpose: ESP32-C3 bootloader initialization entry used by [call_start_cpu0](#call_start_cpu0).

Inputs: none.

Outputs: `ESP_OK` or an ESP-IDF error.

Side effects: configures CPU reset info, hardware, watchdog/reset behavior, flash, console, and other bootloader prerequisites.

### Application Startup

<a id="application-call_start_cpu0"></a>

### Application call_start_cpu0

Definition: `logOS/main/components/logos_cpu_start.c:726` when this source is linked.

Purpose: custom ESP32-C3 app startup entry. It is based on ESP-IDF `cpu_start.c` but changes the final handoff to enter logOS directly instead of the normal ESP-IDF `start_cpu0` and main-task path.

Inputs: none from C callers; entered from the second-stage bootloader.

Outputs: does not return.

Side effects: initializes CPU exception state, BSS, early memory, RTC, MSPI/cache/flash state, console UART timing, and early system hardware. It then calls [kernel_start](#kernel_start) directly.

Important callees: [core_intr_matrix_clear](#core_intr_matrix_clear), [init_cpu](#init_cpu), [get_reset_reason](#get_reset_reason), [init_bss](#init_bss), [ext_mem_init](#ext_mem_init), [init_pre_logos_tls_area](#init_pre_logos_tls_area), [sys_rtc_init](#sys_rtc_init), [mspi_init](#mspi_init), [system_early_init](#system_early_init), [kernel_start](#kernel_start).

### core_intr_matrix_clear

Definition: `logOS/main/components/logos_cpu_start.c:153`.

Purpose: clears interrupt matrix routes during early startup.

Inputs: none.

Outputs: none.

Side effects: routes interrupt sources to invalid interrupt numbers through ROM interrupt-matrix helpers.

### init_cpu

Definition: `logOS/main/components/logos_cpu_start.c:179`.

Purpose: initializes CPU-local exception and low-level runtime state.

Inputs: none.

Outputs: none.

Side effects: configures CPU exception behavior and early RISC-V state.

### get_reset_reason

Definition: `logOS/main/components/logos_cpu_start.c:229`.

Purpose: reads reset reasons for CPU slots.

Inputs: pointer to reset-reason array.

Outputs: fills the array.

Side effects: reads ROM reset-reason state.

### init_bss

Definition: `logOS/main/components/logos_cpu_start.c:240`.

Purpose: initializes zeroed data sections depending on reset reason.

Inputs: reset reason array.

Outputs: none.

Side effects: clears BSS/runtime memory ranges.

### ext_mem_init

Definition: `logOS/main/components/logos_cpu_start.c:268`.

Purpose: early external memory/cache related initialization.

Inputs: none.

Outputs: none.

Side effects: configures memory/cache state before normal code uses flash and RAM services.

### init_pre_logos_tls_area

Definition: `logOS/main/components/logos_cpu_start.c:347`.

Purpose: initializes pre-kernel thread-local storage for the selected CPU.

Inputs: CPU number.

Outputs: none.

Side effects: sets TLS area used before FreeRTOS or logOS process abstractions exist.

### sys_rtc_init

Definition: `logOS/main/components/logos_cpu_start.c:363`.

Purpose: initializes RTC/clock-related system state.

Inputs: reset reason array.

Outputs: none.

Side effects: configures RTC and clock state.

### flash_init_state

Definition: `logOS/main/components/logos_cpu_start.c:398`.

Purpose: prepares flash driver/cache state for app execution.

Inputs: none.

Outputs: none.

Side effects: updates ESP-IDF flash/cache globals.

### mspi_init

Definition: `logOS/main/components/logos_cpu_start.c:414`.

Purpose: initializes MSPI/flash access for the app.

Inputs: none.

Outputs: none.

Side effects: configures MSPI, flash MMU, and cache-related state.

### system_early_init

Definition: `logOS/main/components/logos_cpu_start.c:460`.

Purpose: performs the subset of ESP-IDF early system setup needed before logOS takes control.

Inputs: reset reason array.

Outputs: none.

Side effects: configures low-level clocks, console UART baud rate, safety checks, and reset paths.

### kernel_start

Definition: `logOS/main/logOS.c:9`.

Purpose: logOS kernel entry used by the custom CPU-start path.

Inputs: none.

Outputs: never returns under normal operation.

Side effects: installs the trap vector with [trap_install](#trap_install), initializes/mounts the filesystem through [logos_start](#logos_start), and enters the shell through [logos_shell_run](#logos_shell_run). If the shell returns, it executes a `wfi` loop.

Important callees: [trap_install](#trap_install), [logos_start](#logos_start), [logos_shell_run](#logos_shell_run).

### app_main

Definition: `logOS/main/logOS.c:28`.

Purpose: normal ESP-IDF app entry fallback.

Inputs: none.

Outputs: returns only if [logos_shell_run](#logos_shell_run) returns, which it currently does not.

Side effects: starts logOS services and shell through [logos_start](#logos_start) and [logos_shell_run](#logos_shell_run). This path does not call [trap_install](#trap_install) itself.

<a id="chapter-3-application-flows"></a>

## [Chapter 3: Application Flows](#chapter-3-application-flows)

Chapter Purpose: Explains the major runtime paths end to end so the function catalog can be read in terms of behavior, not only source-file ownership.

### Power-On to logOS Shell

Call flow: ESP32-C3 ROM bootloader -> custom second-stage bootloader [Bootloader call_start_cpu0](#bootloader-call_start_cpu0) -> application startup [Application call_start_cpu0](#application-call_start_cpu0) when linked -> [kernel_start](#kernel_start) -> [trap_install](#trap_install) -> [logos_start](#logos_start) -> [logos_shell_run](#logos_shell_run).

The ROM bootloader loads the second-stage bootloader from flash. The custom second-stage entry initializes bootloader services, chooses an application partition through [select_partition_number](#select_partition_number), loads the image through ESP-IDF bootloader support, and jumps to the app image. If `logos_cpu_start.c` is linked as the app startup object, the application startup performs the required low-level CPU, BSS, memory, RTC, MSPI/cache, flash, and early hardware setup, then calls [kernel_start](#kernel_start) directly.

That direct jump is the point where logOS intentionally differs from normal ESP-IDF startup. Normal ESP-IDF startup reaches `start_cpu0`, walks `SYS_STARTUP_FN()`/`ESP_SYSTEM_INIT_FN(...)` component initialization, starts FreeRTOS, and eventually calls [app_main](#app_main) from the main task. The custom startup path bypasses that main-task flow, so logOS installs traps in [kernel_start](#kernel_start), initializes its own runtime, and enters the shell without depending on the FreeRTOS `main_task` handoff. In the current `main/CMakeLists.txt`, `logos_cpu_start.c` is not compiled, so [app_main](#app_main) remains the normal ESP-IDF fallback entry in this tree.

### Filesystem Mount or Format

Call flow: [logos_start](#logos_start) -> [block_init](#block_init) -> [logos_partition_init](#logos_partition_init) -> [fs_mount](#fs_mount); on mount failure, [block_count](#block_count) -> [fs_format](#fs_format) -> [seed_filesystem](#seed_filesystem) -> [fs_mount](#fs_mount) -> [print_hello_file](#print_hello_file).

[logos_start](#logos_start) waits briefly, initializes the fixed flash-backed block layer, then attempts to mount the logOS filesystem. [block_init](#block_init) delegates to [logos_partition_init](#logos_partition_init), which initializes the default flash chip on demand before using raw flash reads, writes, and erases. That on-demand flash initialization matters because the custom kernel startup path may skip ESP-IDF startup hooks that normally initialize flash services.

If [fs_mount](#fs_mount) cannot recognize the filesystem layout, [logos_start](#logos_start) formats the whole `logosfs` block range using [fs_format](#fs_format), seeds `/etc/hello.txt` through [seed_filesystem](#seed_filesystem), remounts, and prints the file through [print_hello_file](#print_hello_file). This makes first boot self-initializing while preserving the normal mount path for later boots.

### Shell Input and Output

Call flow: [logos_shell_run](#logos_shell_run) -> [read_line](#read_line) -> [logos_getchar](#logos_getchar) / [logos_putchar](#logos_putchar) / [logos_write](#logos_write) -> [logos_syscall](#logos_syscall) -> [trap_handler](#trap_handler) -> [c_trap_handler](#c_trap_handler) -> [syscall_dispatch](#syscall_dispatch) -> console backend.

[logos_shell_run](#logos_shell_run) is currently a simple built-in shell loop. It prints prompts and messages through [shell_write](#shell_write), which uses [logos_write](#logos_write) on fd 1. [read_line](#read_line) polls input with [logos_getchar](#logos_getchar), echoes printable characters with [logos_putchar](#logos_putchar), and handles backspace locally. The user syscall wrappers place arguments in RISC-V registers and execute `ecall`; the trap path dispatches to the kernel, where console fds are backed by the character-device path and ROM console functions.

Current built-ins are `help` and `clear`; unknown commands are reported as unsupported text. The shell does not yet parse external commands, pipelines, redirection, job control, or arguments for built-ins.

### Syscall Dispatch

Call flow: user wrapper -> `ecall` in [logos_syscall](#logos_syscall) -> [trap_handler](#trap_handler) -> [c_trap_handler](#c_trap_handler) -> [syscall_dispatch](#syscall_dispatch) -> filesystem, device, process, or environment helper.

The user wrappers [logos_putchar](#logos_putchar), [logos_getchar](#logos_getchar), and [logos_write](#logos_write) are thin convenience calls over [logos_syscall](#logos_syscall). The assembly trap entry saves the interrupted register state, switches to the trap stack, and enters the C trap handler. [c_trap_handler](#c_trap_handler) recognizes machine-mode ecalls, delegates to [syscall_dispatch](#syscall_dispatch), stores return values back into the trap frame, and resumes with [trap_ret](#trap_ret).

After a handled `ecall`, [c_trap_handler](#c_trap_handler) advances `mepc` so the returning context continues after the `ecall` instruction instead of executing the same syscall again.

### File Read/Write Path

Read/write flow: [sys_open](#sys_open) / [fs_open](#fs_open) -> process fd table via [fd_alloc](#fd_alloc) and [get_fd](#get_fd) -> [sys_read](#sys_read) / [sys_write](#sys_write) -> [file_read](#file_read) / [file_write](#file_write) -> [block_read](#block_read) / [block_write](#block_write) -> [logos_partition_read](#logos_partition_read) / [logos_partition_write](#logos_partition_write) / [logos_partition_erase](#logos_partition_erase) -> ESP flash APIs.

[sys_open](#sys_open) resolves the path, opens the filesystem node, allocates an fd, and records type, inode, offset, and device metadata in the current process. Reads and writes use [get_fd](#get_fd) to validate the descriptor. Regular-file reads and writes pass through [file_read](#file_read) and [file_write](#file_write); character-device fds route to [device_read](#device_read) or [device_write](#device_write).

The filesystem uses direct block references documented in the inode/file helpers. Flash writes are mediated by [block_write](#block_write) and the fixed partition layer. Because NOR flash erases by sector rather than byte range, the block layer performs sector read-modify-erase-write work before calling the partition write APIs.

### Process Spawn and Exit

Call flow: [sys_spawn](#sys_spawn) -> [proc_alloc](#proc_alloc) -> environment and fd setup through [proc_env_copy](#proc_env_copy), [proc_env_init](#proc_env_init), and [proc_fd_init](#proc_fd_init) -> [elf_load_at](#elf_load_at) -> child trap frame -> [trap_ret](#trap_ret); child [sys_exit](#sys_exit) resumes parent and updates `?`.

[sys_spawn](#sys_spawn) resolves the executable path, allocates a process slot, copies or initializes environment state, initializes child fds, loads the ELF image at the child's memory base, constructs a user stack, and switches `current_proc` to the child. It then enters the child by restoring the child's trap frame through [trap_ret](#trap_ret).

When the child exits, [sys_exit](#sys_exit) records the exit status, resumes the saved parent trap frame, updates the parent's `?` environment variable through [proc_set_env_int](#proc_set_env_int), and frees or marks process state as appropriate.

### ELF Load Path

Call flow: [elf_load_at](#elf_load_at) -> [validate_elf_header](#validate_elf_header) -> PT_LOAD segment copy through [file_load_direct](#file_load_direct) -> BSS zero -> [apply_relocations](#apply_relocations) for PIE images.

The loader validates the ELF header before trusting offsets or sizes. Loadable segments are copied directly from the file into the target process memory range, zero-fill covers BSS past file-backed data, and relocation handling adjusts position-independent images for the selected load base. [elf_load](#elf_load) and [elf_exec](#elf_exec) are convenience entry points around the same loader mechanics, while [elf_trampoline](#elf_trampoline) is the assembly transition helper.

### Standalone `/boot/kernel` Reader

Call flow: [_start](#_start) -> [boot_main](#boot_main) -> [boot_resolve_path](#boot_resolve_path) -> [boot_file_read](#boot_file_read) and direct block reads -> jump to `KERNEL_LOAD_ADDR`.

The standalone boot filesystem reader can locate `/boot/kernel`, read inode and directory data through its own minimal filesystem routines, copy the kernel image to `KERNEL_LOAD_ADDR`, and jump there. It has its own console and string helpers so it can run without the main kernel runtime.

This code is present in `logOS/main/components/boot`, but it is not compiled by the current `logOS/main/CMakeLists.txt`. Treat it as a documented boot-reader implementation in the tree, not an active part of the current app image.

<a id="chapter-4-kernel-runtime"></a>

## [Chapter 4: Kernel Runtime](#chapter-4-kernel-runtime)

Chapter Purpose: Covers the logOS runtime initialization, shell loop, and small kernel runtime helpers that sit above boot and below user-facing services.

### logos_strlen

Definition: `logOS/main/logos_start.c:11`.

Purpose: small local string-length helper used before relying on broader C library behavior.

Inputs: NUL-terminated string.

Outputs: byte length before NUL.

Side effects: none.

### seed_filesystem

Definition: `logOS/main/logos_start.c:23`.

Purpose: creates `/etc/hello.txt` with a fixed demo message after formatting.

Inputs: none.

Outputs: none.

Side effects: creates `/etc`, creates `hello.txt`, writes file data, and prints status.

Important callees: [fs_mkdir](#fs_mkdir), [file_create](#file_create), [file_write](#file_write), [kernel_console_printf](#kernel_console_printf).

### print_hello_file

Definition: `logOS/main/logos_start.c:51`.

Purpose: smoke-tests the mounted filesystem by opening and printing `/etc/hello.txt`.

Inputs: none.

Outputs: none.

Side effects: reads from filesystem and writes to kernel console.

Important callees: [fs_open](#fs_open), [file_read](#file_read), [kernel_console_printf](#kernel_console_printf).

### logos_start

Definition: `logOS/main/logos_start.c:74`.

Purpose: initializes the logOS filesystem stack and demo content.

Inputs: none.

Outputs: none.

Side effects: waits one second, initializes the flash-backed block layer, mounts the filesystem, formats it on first boot or incompatible layout, seeds demo content after format, and prints `/etc/hello.txt`.

Important callees: [kernel_delay_ms](#kernel_delay_ms), [block_init](#block_init), [block_count](#block_count), [fs_mount](#fs_mount), [fs_format](#fs_format), [seed_filesystem](#seed_filesystem), [print_hello_file](#print_hello_file).

### shell_write

Definition: `logOS/main/components/shell/shell.c:9`.

Purpose: writes shell output through the user syscall shim.

Inputs: NUL-terminated string.

Outputs: none.

Side effects: invokes [logos_write](#logos_write) on fd 1.

### read_line

Definition: `logOS/main/components/shell/shell.c:15`.

Purpose: blocking line editor for the single-threaded shell.

Inputs: destination buffer and capacity.

Outputs: fills buffer with a NUL-terminated command line.

Side effects: reads characters via [logos_getchar](#logos_getchar), echoes printable input with [logos_putchar](#logos_putchar), handles backspace, and delays between polling iterations.

### logos_shell_run

Definition: `logOS/main/components/shell/shell.c:46`.

Purpose: runs the built-in logOS shell.

Inputs: none.

Outputs: never returns in current code.

Side effects: prints the prompt, reads commands, implements `help` and `clear`, and reports unknown commands.

Important callees: [shell_write](#shell_write), [read_line](#read_line).

### kernel

Definition: `logOS/main/components/kernel/main.c:5`.

Purpose: initializes kernel subsystems that are currently separate from [kernel_start](#kernel_start).

Inputs: none.

Outputs: `0` on success or an error from [console_dev_init](#console_dev_init).

Side effects: registers the console character device and initializes the process table.

Important callees: [console_dev_init](#console_dev_init), [proc_init](#proc_init), [kernel_console_printf](#kernel_console_printf).

### kernel_delay_ms

Definition: `logOS/main/components/kernel/kernel_time.c:6`.

Purpose: millisecond delay helper.

Inputs: milliseconds.

Outputs: none.

Side effects: busy-waits through `esp_rom_delay_us` in <=1000 ms chunks.

<a id="chapter-5-storage-and-filesystem"></a>

## [Chapter 5: Storage and Filesystem](#chapter-5-storage-and-filesystem)

Chapter Purpose: Covers fixed flash partition access, the block layer, filesystem formatting and mounting, bitmap and inode allocation, directory operations, and regular file data paths.

### check_range

Definition: `logOS/main/components/kernel/logos_partition.c:11`.

Purpose: validates a relative access against the fixed `logosfs` partition size.

Inputs: offset and length.

Outputs: `0` if valid, `-1` otherwise.

Side effects: none.

### logos_partition_init

Definition: `logOS/main/components/kernel/logos_partition.c:24`.

Purpose: initializes ESP-IDF default flash chip support on demand.

Inputs: none.

Outputs: `0` on success, `-1` on flash initialization failure.

Side effects: calls `esp_flash_init_default_chip` if the default chip driver has not already been initialized; prints errors through [kernel_console_printf](#kernel_console_printf).

### logos_partition_read

Definition: `logOS/main/components/kernel/logos_partition.c:48`.

Purpose: reads bytes from the fixed `logosfs` partition.

Inputs: relative offset, destination buffer, length.

Outputs: `0` on success, `-1` on invalid range/buffer or flash read failure.

Side effects: calls `esp_flash_read` at physical flash offset `LOGOS_PARTITION_OFFSET + offset`.

### logos_partition_write

Definition: `logOS/main/components/kernel/logos_partition.c:60`.

Purpose: writes bytes to the fixed `logosfs` partition.

Inputs: relative offset, source buffer, length.

Outputs: `0` on success, `-1` on invalid range/buffer or flash write failure.

Side effects: calls `esp_flash_write`. Caller must erase sectors first.

### logos_partition_erase

Definition: `logOS/main/components/kernel/logos_partition.c:72`.

Purpose: erases a sector-aligned range in the fixed `logosfs` partition.

Inputs: relative offset and length.

Outputs: `0` on success, `-1` on invalid range/alignment or flash erase failure.

Side effects: calls `esp_flash_erase_region`.

### logos_partition_size

Definition: `logOS/main/components/kernel/logos_partition.c:89`.

Purpose: exposes the fixed partition size.

Inputs: none.

Outputs: `LOGOS_PARTITION_SIZE`.

Side effects: none.

### block_init

Definition: `logOS/main/components/kernel/block.c:19`.

Purpose: initializes the flash-backed block device.

Inputs: none.

Outputs: `FS_OK` or `FS_ERR_IO`.

Side effects: calls [logos_partition_init](#logos_partition_init) and sets internal `logosfs_initialized`.

### block_read

Definition: `logOS/main/components/kernel/block.c:32`.

Purpose: reads one filesystem block from `logosfs`.

Inputs: block number and destination buffer.

Outputs: `FS_OK` or `FS_ERR_IO`.

Side effects: reads flash via [logos_partition_read](#logos_partition_read).

### block_write

Definition: `logOS/main/components/kernel/block.c:55`.

Purpose: writes one filesystem block to `logosfs`.

Inputs: block number and source buffer.

Outputs: `FS_OK` or `FS_ERR_IO`.

Side effects: performs flash read-modify-erase-write at sector granularity using [logos_partition_read](#logos_partition_read), [logos_partition_erase](#logos_partition_erase), and [logos_partition_write](#logos_partition_write).

### block_count

Definition: `logOS/main/components/kernel/block.c:87`.

Purpose: returns the number of 512-byte filesystem blocks available.

Inputs: none.

Outputs: block count, or `0` if the block device is not initialized.

Side effects: none.

### fs_format

Definition: `logOS/main/components/kernel/fs.c:18`.

Purpose: creates a fresh filesystem layout.

Inputs: total block count.

Outputs: `FS_OK` or error.

Side effects: writes superblock, bitmap blocks, inode table, root inode, and root `.`/`..` entries.

Important callees: [block_write](#block_write), [inode_write](#inode_write), [dir_add](#dir_add).

### fs_mount

Definition: `logOS/main/components/kernel/fs.c:101`.

Purpose: mounts an existing filesystem by reading and validating the superblock.

Inputs: none.

Outputs: `FS_OK`, `FS_ERR_IO`, or `FS_ERR_INVALID`.

Side effects: updates global `sb`.

### fs_open

Definition: `logOS/main/components/kernel/fs.c:121`.

Purpose: resolves an absolute path to an inode number.

Inputs: path and output inode pointer.

Outputs: `FS_OK` or filesystem error.

Side effects: none beyond block reads.

Important callees: [dir_lookup](#dir_lookup), [inode_read](#inode_read).

### fs_root_inode

Definition: `logOS/main/components/kernel/fs.c:189`.

Purpose: returns the root inode number.

Inputs: none.

Outputs: `ROOT_INODE`.

Side effects: none.

### fs_mknod

Definition: `logOS/main/components/kernel/fs.c:201`.

Purpose: creates a character device inode in a directory.

Inputs: parent directory inode, name, major number, minor number.

Outputs: new inode number or negative error.

Side effects: allocates an inode and adds a directory entry.

### bitmap_get

Definition: `logOS/main/components/kernel/fs_alloc.c:7`.

Purpose: reads one allocation bit for a block.

Inputs: block number.

Outputs: `0`, `1`, or `FS_ERR_IO`.

Side effects: reads bitmap block into `block_buf`.

### bitmap_set

Definition: `logOS/main/components/kernel/fs_alloc.c:20`.

Purpose: changes one allocation bit for a block.

Inputs: block number and boolean value.

Outputs: `FS_OK` or `FS_ERR_IO`.

Side effects: writes the bitmap block.

### block_alloc

Definition: `logOS/main/components/kernel/fs_alloc.c:39`.

Purpose: allocates and zeroes one data block.

Inputs: none.

Outputs: allocated block number or negative error.

Side effects: updates bitmap, decrements `sb.free_blocks`, writes superblock, and clears the allocated block.

### block_free

Definition: `logOS/main/components/kernel/fs_alloc.c:70`.

Purpose: frees one data block.

Inputs: block number.

Outputs: `FS_OK` or error.

Side effects: clears bitmap bit, increments `sb.free_blocks`, and writes superblock.

### inode_read

Definition: `logOS/main/components/kernel/inode.c:11`.

Purpose: reads an inode from the inode table.

Inputs: inode number and output inode pointer.

Outputs: `FS_OK`, `FS_ERR_INVALID`, or `FS_ERR_IO`.

Side effects: reads a containing inode block into `block_buf`.

### inode_write

Definition: `logOS/main/components/kernel/inode.c:27`.

Purpose: writes an inode into the inode table.

Inputs: inode number and source inode pointer.

Outputs: `FS_OK`, `FS_ERR_INVALID`, or `FS_ERR_IO`.

Side effects: read-modify-writes a containing inode block.

### inode_alloc

Definition: `logOS/main/components/kernel/inode.c:43`.

Purpose: allocates a free inode.

Inputs: none.

Outputs: inode number or negative error.

Side effects: marks inode in use, initializes link count, decrements `sb.free_inodes`, and writes superblock.

### inode_free

Definition: `logOS/main/components/kernel/inode.c:71`.

Purpose: frees an inode and its direct data blocks.

Inputs: inode number.

Outputs: filesystem status.

Side effects: calls [block_free](#block_free), clears inode, increments `sb.free_inodes`, and writes superblock.

### inode_get_type

Definition: `logOS/main/components/kernel/inode.c:97`.

Purpose: returns an inode type.

Inputs: inode number and output pointer.

Outputs: filesystem status.

Side effects: reads inode.

### inode_get_size

Definition: `logOS/main/components/kernel/inode.c:108`.

Purpose: returns an inode size.

Inputs: inode number and output pointer.

Outputs: filesystem status.

Side effects: reads inode.

### inode_get_device

Definition: `logOS/main/components/kernel/inode.c:119`.

Purpose: returns character-device major/minor numbers from an inode.

Inputs: inode number and output pointers.

Outputs: filesystem status.

Side effects: reads inode.

### dir_lookup

Definition: `logOS/main/components/kernel/dir.c:11`.

Purpose: finds a name in a directory inode.

Inputs: directory inode, name, output inode pointer.

Outputs: `FS_OK`, `FS_ERR_NOT_DIR`, `FS_ERR_NOT_FOUND`, or `FS_ERR_IO`.

Side effects: reads directory data blocks.

### dir_add

Definition: `logOS/main/components/kernel/dir.c:45`.

Purpose: adds a directory entry.

Inputs: directory inode, name, inode number to insert.

Outputs: filesystem status.

Side effects: may allocate a new directory block and updates directory size.

### dir_remove

Definition: `logOS/main/components/kernel/dir.c:124`.

Purpose: removes a directory entry by name.

Inputs: directory inode and name.

Outputs: filesystem status.

Side effects: marks entry free and decrements directory size.

### dir_list

Definition: `logOS/main/components/kernel/dir.c:163`.

Purpose: enumerates directory entries.

Inputs: directory inode, entry buffer, max entries, output total count.

Outputs: filesystem status.

Side effects: reads directory blocks.

### dir_is_empty

Definition: `logOS/main/components/kernel/dir.c:199`.

Purpose: checks whether a directory has entries other than `.` and `..`.

Inputs: directory inode.

Outputs: `1` if empty, `0` otherwise.

Side effects: reads directory blocks.

### fs_mkdir

Definition: `logOS/main/components/kernel/dir.c:226`.

Purpose: creates a directory under a parent directory.

Inputs: parent inode and name.

Outputs: new inode number or negative error.

Side effects: allocates inode, creates `.`/`..`, and inserts the name in the parent directory.

### fs_rmdir

Definition: `logOS/main/components/kernel/dir.c:276`.

Purpose: removes an empty directory.

Inputs: parent inode and name.

Outputs: filesystem status.

Side effects: removes child `.`/`..`, removes parent entry, and frees child inode.

### file_create

Definition: `logOS/main/components/kernel/file.c:13`.

Purpose: creates a regular file in a directory.

Inputs: parent directory inode and name.

Outputs: new inode number or negative error.

Side effects: allocates inode and adds directory entry.

### file_read

Definition: `logOS/main/components/kernel/file.c:46`.

Purpose: reads from a regular file or character device inode.

Inputs: inode, offset, destination buffer, length.

Outputs: bytes read or negative error.

Side effects: for devices, dispatches to [device_read](#device_read); for files, reads direct blocks into `block_buf`.

### file_load_direct

Definition: `logOS/main/components/kernel/file.c:105`.

Purpose: optimized file read for ELF segment loading.

Inputs: inode, offset, destination buffer, length.

Outputs: bytes read or negative error.

Side effects: reads full aligned blocks directly to destination when possible; otherwise uses `block_buf`.

### file_write

Definition: `logOS/main/components/kernel/file.c:160`.

Purpose: writes to a regular file or character device inode.

Inputs: inode, offset, source buffer, length.

Outputs: bytes written or negative error.

Side effects: for devices, dispatches to [device_write](#device_write); for files, allocates blocks as needed, writes block data, and updates inode size.

### file_truncate

Definition: `logOS/main/components/kernel/file.c:232`.

Purpose: changes a regular file size.

Inputs: inode and new size.

Outputs: filesystem status.

Side effects: frees direct blocks beyond the new end and updates inode size.

### file_delete

Definition: `logOS/main/components/kernel/file.c:256`.

Purpose: deletes a file or character device directory entry.

Inputs: parent directory inode and name.

Outputs: filesystem status.

Side effects: removes directory entry, decrements link count, or frees inode.

<a id="chapter-6-processes-syscalls-and-traps"></a>

## [Chapter 6: Processes, Syscalls, and Traps](#chapter-6-processes-syscalls-and-traps)

Chapter Purpose: Covers the RISC-V trap path, user syscall wrappers, syscall dispatcher internals, process slots, file descriptors, and process environment helpers.

### trap_install

Definition: `logOS/main/components/kernel/trap.c:16`.

Purpose: installs the RISC-V trap entry and initial kernel trap frame.

Inputs: none.

Outputs: none.

Side effects: disables global interrupts and writes `mtvec`/`mscratch` through [set_trap_handler](#set_trap_handler).

### c_trap_handler

Definition: `logOS/main/components/kernel/trap.c:28`.

Purpose: C-level trap handler for ecalls and fatal traps.

Inputs: trap frame pointer.

Outputs: normally resumes through [trap_ret](#trap_ret) for handled ecalls; does not return for unhandled traps.

Side effects: prints trap diagnostics, calls [syscall_dispatch](#syscall_dispatch), advances `mepc` after handled ecalls, and halts on unhandled traps.

### set_trap_handler

Definition: `logOS/main/components/kernel/trap.S:55`.

Purpose: writes the trap entry address to `mtvec` and active frame pointer to `mscratch`.

Inputs: handler pointer and trap-frame pointer.

Outputs: none.

Side effects: writes RISC-V CSRs.

### set_mie

Definition: `logOS/main/components/kernel/trap.S:62`.

Purpose: writes the machine interrupt-enable CSR.

Inputs: new `mie` value.

Outputs: none.

Side effects: writes `mie`.

### get_mie

Definition: `logOS/main/components/kernel/trap.S:68`.

Purpose: reads the machine interrupt-enable CSR.

Inputs: none.

Outputs: `mie` value.

Side effects: none.

### set_mstatus_bit

Definition: `logOS/main/components/kernel/trap.S:74`.

Purpose: sets bits in `mstatus`.

Inputs: bit mask.

Outputs: none.

Side effects: writes `mstatus`.

### enable_interrupts

Definition: `logOS/main/components/kernel/trap.S:80`.

Purpose: sets the machine interrupt-enable bit in `mstatus`.

Inputs: none.

Outputs: none.

Side effects: enables machine interrupts.

### disable_interrupts

Definition: `logOS/main/components/kernel/trap.S:86`.

Purpose: clears the machine interrupt-enable bit in `mstatus`.

Inputs: none.

Outputs: none.

Side effects: disables machine interrupts.

### get_mcause

Definition: `logOS/main/components/kernel/trap.S:92`.

Purpose: reads machine trap cause.

Inputs: none.

Outputs: `mcause`.

Side effects: none.

### kernel_vector_table

Definition: `logOS/main/components/kernel/trap.S:99`.

Purpose: vector-table entry that jumps to [trap_handler](#trap_handler).

Inputs: trap-time CPU state.

Outputs: none directly.

Side effects: transfers to trap handler.

### trap_handler

Definition: `logOS/main/components/kernel/trap.S:105`.

Purpose: assembly trap entry that saves RISC-V register state.

Inputs: interrupted CPU state and active trap frame in `mscratch`.

Outputs: enters C trap handler.

Side effects: saves registers and CSRs to the trap frame, switches to the trap stack, and calls `c_trap`.

### trap_ret

Definition: `logOS/main/components/kernel/trap.S:165`.

Purpose: restores a trap frame and resumes execution at `mepc`.

Inputs: trap-frame pointer in `a0`.

Outputs: does not return as a normal C function.

Side effects: restores registers, writes `mepc`/`mstatus`/`mscratch`, executes `fence.i`, and jumps to restored PC.

### run_user_program

Definition: `logOS/main/components/kernel/trap.S:215`.

Purpose: saves kernel callee-saved context and enters a user trap frame through [trap_ret](#trap_ret).

Inputs: trap-frame pointer in `a0`.

Outputs: returns to kernel when trap code restores `kernel_context`.

Side effects: stores kernel stack context globally.

### debug_user_entry

Definition: `logOS/main/components/kernel/trap.S:257`.

Purpose: debug entry that issues syscall number `99` and loops.

Inputs: none.

Outputs: none.

Side effects: triggers an `ecall`.

### logos_syscall

Definition: `logOS/main/components/user/user_syscall.c:6`.

Purpose: user-side RISC-V syscall shim.

Inputs: syscall number and up to three arguments.

Outputs: syscall return value from `a0`.

Side effects: executes `ecall` with `a0`/`a1`/`a2`/`a7` populated.

### logos_putchar

Definition: `logOS/main/components/user/user_syscall.c:26`.

Purpose: user-facing putchar wrapper.

Inputs: character.

Outputs: kernel syscall result.

Side effects: calls [logos_syscall](#logos_syscall).

### logos_getchar

Definition: `logOS/main/components/user/user_syscall.c:30`.

Purpose: user-facing getchar wrapper.

Inputs: none.

Outputs: character or syscall error.

Side effects: calls [logos_syscall](#logos_syscall).

### logos_write

Definition: `logOS/main/components/user/user_syscall.c:34`.

Purpose: user-facing write wrapper.

Inputs: fd, buffer, length.

Outputs: kernel syscall result.

Side effects: calls [logos_syscall](#logos_syscall).

### syscall_dispatch

Definition: `logOS/main/components/kernel/syscall.c:960`.

Purpose: dispatches kernel syscalls by `tf->a7`.

Inputs: trap frame with syscall number and arguments.

Outputs: `1` if a top-level process exit should stop resume handling; otherwise `0` after storing syscall result in `tf->a0`.

Side effects: may read/write files, mutate process table/env/fds, create directories/devices, load ELF programs, switch processes via [trap_ret](#trap_ret), or update the current working directory.

Handled syscalls: `SYS_exit`, `SYS_read`, `SYS_write`, `SYS_open`, `SYS_close`, `SYS_spawn`, `SYS_readdir`, `SYS_mkdir`, `SYS_rmdir`, `SYS_mknod`, `SYS_setenv`, `SYS_getenv`, `SYS_unsetenv`, `SYS_getenv_count`, `SYS_getenv_entry`, `SYS_chdir`, `SYS_unlink`, `SYS_link`, `SYS_rename`, `SYS_stat`.

### get_fd

Definition: `logOS/main/components/kernel/syscall.c:27`.

Purpose: resolves an fd in the current process table entry.

Inputs: fd number.

Outputs: pointer to `struct fd_entry` or null.

Side effects: none.

### fd_alloc

Definition: `logOS/main/components/kernel/syscall.c:39`.

Purpose: allocates an fd >= 3 in the current process.

Inputs: none.

Outputs: fd number or `-1`.

Side effects: marks the fd slot in use.

### sys_exit

Definition: `logOS/main/components/kernel/syscall.c:61`.

Purpose: terminates the current process.

Inputs: trap frame with exit status in `a0`.

Outputs: `1` for top-level process exit; child exits resume the parent and do not return.

Side effects: updates exit code, parent trap frame, process states, `current_proc`, and `?` environment variable.

### sys_read

Definition: `logOS/main/components/kernel/syscall.c:98`.

Purpose: reads from an open fd.

Inputs: trap frame `a0=fd`, `a1=buf`, `a2=len`.

Outputs: bytes read or negative error.

Side effects: advances fd offset for regular files.

### sys_write

Definition: `logOS/main/components/kernel/syscall.c:131`.

Purpose: writes to an open fd.

Inputs: trap frame `a0=fd`, `a1=buf`, `a2=len`.

Outputs: bytes written or negative error.

Side effects: writes device/file data and advances fd offset for regular files.

### canonicalize_path

Definition: `logOS/main/components/kernel/syscall.c:164`.

Purpose: removes `.` and `..` path components in-place.

Inputs: absolute path buffer.

Outputs: modified path buffer.

Side effects: uses a static temporary buffer.

### resolve_path

Definition: `logOS/main/components/kernel/syscall.c:212`.

Purpose: converts absolute or relative paths to canonical absolute paths.

Inputs: source path, destination buffer, destination size.

Outputs: `0` or `-1`.

Side effects: reads current process `CWD` environment variable.

### sys_open

Definition: `logOS/main/components/kernel/syscall.c:253`.

Purpose: opens a filesystem node and creates an fd.

Inputs: trap frame `a0=path`, `a1=flags`.

Outputs: fd or negative error.

Side effects: resolves path, allocates fd, initializes fd metadata.

### sys_close

Definition: `logOS/main/components/kernel/syscall.c:306`.

Purpose: closes an fd >= 3.

Inputs: trap frame `a0=fd`.

Outputs: `0` or `-1`.

Side effects: clears fd in-use flag.

### sys_spawn

Definition: `logOS/main/components/kernel/syscall.c:337`.

Purpose: loads and runs a child ELF process.

Inputs: trap frame `a0=path`, `a1=argv`, `a2=envp`.

Outputs: on load failure, negative error. On success, starts child via [trap_ret](#trap_ret) and later resumes parent with child's exit code.

Side effects: copies args/env, allocates process slot, loads ELF through [elf_load_at](#elf_load_at), builds child stack, initializes child fds, mutates process states, and switches `current_proc`.

### resolve_parent

Definition: `logOS/main/components/kernel/syscall.c:485`.

Purpose: splits an absolute path into parent inode and final name.

Inputs: absolute path, parent inode output, name output.

Outputs: filesystem status.

Side effects: opens parent path.

### sys_readdir

Definition: `logOS/main/components/kernel/syscall.c:533`.

Purpose: lists directory entries.

Inputs: trap frame `a0=path`, `a1=dirent buffer`, `a2=max_entries`.

Outputs: total entry count or negative error.

Side effects: writes caller-provided buffer.

### sys_mkdir

Definition: `logOS/main/components/kernel/syscall.c:566`.

Purpose: creates a directory by path.

Inputs: trap frame `a0=path`.

Outputs: new inode number or negative error.

Side effects: mutates filesystem.

### sys_rmdir

Definition: `logOS/main/components/kernel/syscall.c:590`.

Purpose: removes a directory by path.

Inputs: trap frame `a0=path`.

Outputs: filesystem status.

Side effects: mutates filesystem.

### sys_mknod

Definition: `logOS/main/components/kernel/syscall.c:614`.

Purpose: creates a character-device node.

Inputs: trap frame `a0=path`, `a1=major`, `a2=minor`.

Outputs: new inode number or negative error.

Side effects: mutates filesystem.

### sys_chdir

Definition: `logOS/main/components/kernel/syscall.c:641`.

Purpose: changes the current process working directory.

Inputs: trap frame `a0=path`.

Outputs: `0` or filesystem error.

Side effects: updates `CWD` environment variable.

### sys_setenv

Definition: `logOS/main/components/kernel/syscall.c:679`.

Purpose: sets one environment variable.

Inputs: trap frame `a0=name`, `a1=value`.

Outputs: `0` or `-1`.

Side effects: mutates current process environment table.

### sys_getenv

Definition: `logOS/main/components/kernel/syscall.c:715`.

Purpose: reads an environment variable value.

Inputs: trap frame `a0=name`, `a1=buf`, `a2=buflen`.

Outputs: value length or `-1`.

Side effects: writes output buffer if provided.

### sys_unsetenv

Definition: `logOS/main/components/kernel/syscall.c:748`.

Purpose: removes one environment variable.

Inputs: trap frame `a0=name`.

Outputs: `0` or `-1`.

Side effects: compacts current process environment table.

### sys_getenv_count

Definition: `logOS/main/components/kernel/syscall.c:766`.

Purpose: returns environment variable count.

Inputs: none.

Outputs: count.

Side effects: none.

### sys_getenv_entry

Definition: `logOS/main/components/kernel/syscall.c:775`.

Purpose: copies the Nth `KEY=value` environment entry.

Inputs: trap frame `a0=index`, `a1=buf`, `a2=buflen`.

Outputs: entry length or `-1`.

Side effects: writes output buffer if provided.

### sys_unlink

Definition: `logOS/main/components/kernel/syscall.c:806`.

Purpose: deletes a file by path.

Inputs: trap frame `a0=path`.

Outputs: filesystem status.

Side effects: mutates filesystem.

### sys_link

Definition: `logOS/main/components/kernel/syscall.c:829`.

Purpose: creates a hard link to an existing regular file.

Inputs: trap frame `a0=target`, `a1=linkpath`.

Outputs: filesystem status.

Side effects: adds directory entry and increments link count.

### sys_rename

Definition: `logOS/main/components/kernel/syscall.c:876`.

Purpose: renames or moves a file/directory.

Inputs: trap frame `a0=oldpath`, `a1=newpath`.

Outputs: filesystem status.

Side effects: adds new directory entry, removes old entry, and updates `..` if moving a directory.

### sys_stat

Definition: `logOS/main/components/kernel/syscall.c:931`.

Purpose: returns file metadata.

Inputs: trap frame `a0=path`, `a1=struct stat_info *`.

Outputs: filesystem status.

Side effects: writes the caller's stat buffer.

### proc_init

Definition: `logOS/main/components/kernel/process.c:18`.

Purpose: initializes all process slots.

Inputs: none.

Outputs: none.

Side effects: sets all slots free, assigns memory bases and stack tops, resets `current_proc` globals indirectly through initial state.

### proc_alloc

Definition: `logOS/main/components/kernel/process.c:31`.

Purpose: allocates a process slot.

Inputs: none.

Outputs: slot index or `-1`.

Side effects: assigns PID, state, parent, and exit code.

### proc_free

Definition: `logOS/main/components/kernel/process.c:45`.

Purpose: frees a process slot.

Inputs: slot index.

Outputs: none.

Side effects: marks slot free and clears PID.

### proc_fd_init

Definition: `logOS/main/components/kernel/process.c:50`.

Purpose: initializes a process fd table.

Inputs: slot index.

Outputs: none.

Side effects: clears all fds and maps fd 0/1/2 to console character device.

### proc_env_find

Definition: `logOS/main/components/kernel/process.c:70`.

Purpose: finds an environment variable by name.

Inputs: process slot and variable name.

Outputs: env index or `-1`.

Side effects: none.

### proc_env_init

Definition: `logOS/main/components/kernel/process.c:82`.

Purpose: initializes a process environment.

Inputs: process slot.

Outputs: none.

Side effects: sets `PATH=/bin`, `CWD=/`, and `?=0`.

### proc_env_copy

Definition: `logOS/main/components/kernel/process.c:91`.

Purpose: copies environment from one process slot to another.

Inputs: destination slot and source slot.

Outputs: none.

Side effects: writes destination environment table.

### proc_set_env

Definition: `logOS/main/components/kernel/process.c:101`.

Purpose: sets an environment variable without returning errors.

Inputs: slot, name, value.

Outputs: none.

Side effects: mutates process environment table if there is room.

### proc_set_env_int

Definition: `logOS/main/components/kernel/process.c:123`.

Purpose: integer convenience wrapper for [proc_set_env](#proc_set_env).

Inputs: slot, name, integer value.

Outputs: none.

Side effects: formats integer to local buffer and updates process env.

<a id="chapter-7-elf-loading-and-user-execution"></a>

## [Chapter 7: ELF Loading and User Execution](#chapter-7-elf-loading-and-user-execution)

Chapter Purpose: Covers ELF validation, segment loading, relocation, exec helpers, and the assembly trampoline used to enter loaded user code.

### validate_elf_header

Definition: `logOS/main/components/kernel/loader.c:20`.

Purpose: validates ELF magic, class, endianness, machine, type, and program-header metadata.

Inputs: ELF header pointer.

Outputs: `LOAD_OK` or loader error.

Side effects: none.

### apply_relocations

Definition: `logOS/main/components/kernel/loader.c:62`.

Purpose: applies `R_RISCV_RELATIVE` relocations for loaded PIE/`ET_DYN` executables.

Inputs: inode number, ELF header pointer, load bias.

Outputs: `LOAD_OK` or loader error.

Side effects: writes relocated words in loaded program memory.

### elf_load_at

Definition: `logOS/main/components/kernel/loader.c:177`.

Purpose: loads an ELF executable into a specified memory slot.

Inputs: path, load address, maximum size, program-info output.

Outputs: `LOAD_OK` or loader error.

Side effects: reads executable file, copies PT_LOAD segments into RAM, zeroes BSS, applies relative relocations, and fills `program_info`.

Important callees: [fs_open](#fs_open), [file_read](#file_read), [file_load_direct](#file_load_direct), [validate_elf_header](#validate_elf_header), [apply_relocations](#apply_relocations).

### elf_load

Definition: `logOS/main/components/kernel/loader.c:307`.

Purpose: loads an ELF at default program load region.

Inputs: path and program-info output.

Outputs: loader status.

Side effects: delegates to [elf_load_at](#elf_load_at).

### elf_exec

Definition: `logOS/main/components/kernel/loader.c:315`.

Purpose: executes a loaded program through an assembly trampoline.

Inputs: program-info pointer.

Outputs: trampoline return.

Side effects: transfers control to user code.

### elf_trampoline

Definition: `logOS/main/components/kernel/loader_asm.S:7`.

Purpose: low-level entry trampoline for loaded ELF code.

Inputs: entry address and stack pointer.

Outputs: program return value.

Side effects: switches stack and jumps/calls loaded code.

<a id="chapter-8-console-and-devices"></a>

## [Chapter 8: Console and Devices](#chapter-8-console-and-devices)

Chapter Purpose: Covers the kernel console, console character device, and generic device table used by file descriptors and syscalls.

### kernel_console_putchar

Definition: `logOS/main/components/kernel/console.c:11`.

Purpose: writes one character through the ROM console output path.

Inputs: character.

Outputs: none.

Side effects: calls `esp_rom_output_putc`.

### kernel_console_getchar

Definition: `logOS/main/components/kernel/console.c:16`.

Purpose: blocking polling read from the ROM console input path.

Inputs: none.

Outputs: one input character.

Side effects: spins until `esp_rom_output_rx_one_char` succeeds.

### kernel_console_write

Definition: `logOS/main/components/kernel/console.c:28`.

Purpose: synchronous buffer write to the kernel console.

Inputs: buffer and byte length.

Outputs: bytes written, or `-1` for invalid arguments.

Side effects: writes each byte through [kernel_console_putchar](#kernel_console_putchar).

### kernel_console_printf

Definition: `logOS/main/components/kernel/console.c:44`.

Purpose: formatted output without using Newlib stdio/VFS.

Inputs: printf-style format and arguments.

Outputs: return value from `esp_rom_vprintf`.

Side effects: writes to ROM console.

### console_read

Definition: `logOS/main/components/kernel/console_dev.c:13`.

Purpose: character-device read handler for `/dev/console`.

Inputs: minor number, output buffer, length.

Outputs: bytes read.

Side effects: blocks on console input for each byte.

### console_write

Definition: `logOS/main/components/kernel/console_dev.c:26`.

Purpose: character-device write handler for `/dev/console`.

Inputs: minor number, input buffer, length.

Outputs: bytes written.

Side effects: writes bytes to kernel console.

### console_dev_init

Definition: `logOS/main/components/kernel/console_dev.c:51`.

Purpose: registers the console character-device driver under `CONSOLE_MAJOR`.

Inputs: none.

Outputs: filesystem-style status code.

Side effects: updates the global device table through [device_register](#device_register).

### device_register

Definition: `logOS/main/components/kernel/device.c:11`.

Purpose: installs a driver operation table for a major number.

Inputs: major number and `struct device_ops *`.

Outputs: `FS_OK`, `FS_ERR_INVALID`, or `FS_ERR_EXISTS`.

Side effects: writes the global `device_table`.

### device_unregister

Definition: `logOS/main/components/kernel/device.c:22`.

Purpose: removes a driver registration.

Inputs: major number.

Outputs: `FS_OK` or `FS_ERR_INVALID`.

Side effects: clears an entry in `device_table`.

### device_open

Definition: `logOS/main/components/kernel/device.c:30`.

Purpose: dispatches an open operation to a character-device driver.

Inputs: major and minor numbers.

Outputs: driver return, `FS_OK`, or `FS_ERR_NOT_FOUND`.

Side effects: driver-defined.

### device_close

Definition: `logOS/main/components/kernel/device.c:40`.

Purpose: dispatches a close operation to a character-device driver.

Inputs: major and minor numbers.

Outputs: driver return, `FS_OK`, or `FS_ERR_NOT_FOUND`.

Side effects: driver-defined.

### device_read

Definition: `logOS/main/components/kernel/device.c:50`.

Purpose: dispatches a read operation to a character-device driver.

Inputs: major/minor, output buffer, length.

Outputs: driver return or filesystem-style error.

Side effects: driver-defined.

### device_write

Definition: `logOS/main/components/kernel/device.c:60`.

Purpose: dispatches a write operation to a character-device driver.

Inputs: major/minor, input buffer, length.

Outputs: driver return or filesystem-style error.

Side effects: driver-defined.

<a id="chapter-9-standalone-boot-filesystem-reader"></a>

## [Chapter 9: Standalone Boot Filesystem Reader](#chapter-9-standalone-boot-filesystem-reader)

Chapter Purpose: Documents the standalone `/boot/kernel` reader and its private filesystem, console, and memory helpers. This source is present but not compiled by the current app CMake file.

This chapter covers the standalone boot filesystem reader under `logOS/main/components/boot`. It is present in the tree but is not listed in the current `logOS/main/CMakeLists.txt`, so these entries document available source rather than active app-image code.

### _start

Definitions: `logOS/main/components/boot/boot_crt0.S:14` and `logOS/main/components/kernel/crt0.S:14`.

Purpose: assembly entry for standalone boot/kernel images.

Inputs: reset/loader-provided CPU state.

Outputs: enters the corresponding C main path.

Side effects: sets up stack/runtime state according to the linker script.

### boot_main

Definition: `logOS/main/components/boot/boot_main.c:12`.

Purpose: minimal bootloader that reads `/boot/kernel` from the logOS filesystem into RAM and jumps to it.

Inputs: none.

Outputs: does not return on success.

Side effects: reads superblock and inode/data blocks via [boot_block_read](#boot_block_read), prints status, loads kernel to `KERNEL_LOAD_ADDR`, and jumps to that address.

### boot_inode_read

Definition: `logOS/main/components/boot/boot_fs.c:9`.

Purpose: reads one inode from a filesystem image.

Inputs: inode number, superblock, scratch buffer, output inode.

Outputs: `0` or `-1`.

Side effects: reads a block through [boot_block_read](#boot_block_read).

### boot_dir_lookup

Definition: `logOS/main/components/boot/boot_fs.c:23`.

Purpose: finds a directory entry in the standalone boot reader.

Inputs: directory inode, superblock, scratch buffer, name, output inode pointer.

Outputs: `0` or `-1`.

Side effects: reads directory blocks.

### boot_resolve_path

Definition: `logOS/main/components/boot/boot_fs.c:58`.

Purpose: resolves an absolute path in the standalone boot reader.

Inputs: path, superblock, scratch buffer, output inode pointer.

Outputs: `0` or `-1`.

Side effects: reads inodes/directories.

### boot_file_read

Definition: `logOS/main/components/boot/boot_fs.c:109`.

Purpose: reads file data from an inode in the standalone boot reader.

Inputs: inode, superblock, scratch buffer, offset, destination, length.

Outputs: bytes read or `-1`.

Side effects: reads file blocks and copies into destination.

### boot_block_read

Definition: `logOS/main/components/boot/boot_io.c:22`.

Purpose: reads one block through a memory-mapped block device.

Inputs: block number and destination buffer.

Outputs: `0` or `-1`.

Side effects: writes block-device MMIO registers at base `0x200000` and waits for completion.

### boot_putchar

Definition: `logOS/main/components/boot/boot_io.c:37`.

Purpose: writes one character to the standalone boot console MMIO.

Inputs: character.

Outputs: none.

Side effects: writes MMIO address `0xFFFF000C`.

### boot_puts

Definition: `logOS/main/components/boot/boot_io.c:42`.

Purpose: writes a NUL-terminated string in the standalone boot environment.

Inputs: string.

Outputs: none.

Side effects: calls [boot_putchar](#boot_putchar).

### boot_put_hex

Definition: `logOS/main/components/boot/boot_io.c:48`.

Purpose: prints an unsigned integer in hexadecimal without leading zero padding.

Inputs: 32-bit value.

Outputs: none.

Side effects: writes console MMIO through [boot_putchar](#boot_putchar).

### boot_put_uint

Definition: `logOS/main/components/boot/boot_io.c:61`.

Purpose: prints an unsigned integer in decimal.

Inputs: 32-bit value.

Outputs: none.

Side effects: writes console MMIO through [boot_putchar](#boot_putchar).

### boot_halt

Definition: `logOS/main/components/boot/boot_io.c:77`.

Purpose: prints a halt message and stops.

Inputs: none.

Outputs: never returns.

Side effects: infinite loop.

### boot_memcpy

Definition: `logOS/main/components/boot/boot_string.c:8`.

Purpose: standalone byte copy.

Inputs: destination, source, byte count.

Outputs: none.

Side effects: writes destination range.

### boot_memset

Definition: `logOS/main/components/boot/boot_string.c:16`.

Purpose: standalone byte fill.

Inputs: destination, byte value, byte count.

Outputs: none.

Side effects: writes destination range.

### boot_strcmp

Definition: `logOS/main/components/boot/boot_string.c:23`.

Purpose: standalone string comparison.

Inputs: two strings.

Outputs: signed byte difference at first mismatch or zero.

Side effects: none.

## Alternate Non-Compiled logOS Sources

The following files are present under `logOS/main` but are not part of the current `logOS/main/CMakeLists.txt` source list. They are still useful context because they show earlier or alternate startup/syscall designs.

### kernel_start.c ext_mem_init

Definition: `logOS/main/components/kernel_start.c:86`.

Purpose: alternate cache/MMU HAL initialization for ESP32-C3 flash mappings.

Inputs: none.

Outputs: none.

Side effects: initializes cache and MMU HAL contexts.

### kernel_start.c sys_rtc_init

Definition: `logOS/main/components/kernel_start.c:121`.

Purpose: alternate RTC initialization helper.

Inputs: reset reason array.

Outputs: none.

Side effects: may disable RTC WDT after watchdog reset and calls `esp_rtc_init`.

### kernel_start.c flash_init_state

Definition: `logOS/main/components/kernel_start.c:141`.

Purpose: alternate flash chip state/timing initialization helper.

Inputs: none.

Outputs: none.

Side effects: calls ESP-IDF SPI flash chip-state and MSPI timing functions.

### kernel_start.c mspi_init

Definition: `logOS/main/components/kernel_start.c:157`.

Purpose: alternate MSPI/flash mapping initialization.

Inputs: none.

Outputs: none.

Side effects: initializes MSPI pins, updates bootloader flash ID, initializes MMU map support, and handles PSRAM initialization when configured.

### kernel_start.c system_early_init

Definition: `logOS/main/components/kernel_start.c:203`.

Purpose: alternate early ESP-IDF system initialization path.

Inputs: reset reason array.

Outputs: none.

Side effects: reserves MSPI pins, initializes memory/clock tree, configures UART baud rate, initializes cache error interrupts, checks memory protection, and validates app image header.

### kernel_start.c call_start_cpu0

Definition: `logOS/main/components/kernel_start.c:362`.

Purpose: alternate ESP32-C3 app entry. Unlike `logos_cpu_start.c`, this version ends by calling `SYS_STARTUP_FN()`, which continues into normal ESP-IDF system startup.

Inputs: none.

Outputs: does not return in normal startup.

Side effects: performs early CPU/BSS/cache/RTC/MSPI/system initialization and then delegates to ESP-IDF startup.

### early syscall_dispatch

Definition: `logOS/main/components/kernel/syscalls/syscall.c:4`.

Purpose: older minimal dispatcher for console-only syscall numbers.

Inputs: trap frame.

Outputs: stores result in `tf->a0`.

Side effects: dispatches only `SYS_PUTCHAR`, `SYS_GETCHAR`, and `SYS_WRITE`.

### sys_getchar

Definition: `logOS/main/components/kernel/syscalls/sys_console.c:7`.

Purpose: older console syscall wrapper around [kernel_console_getchar](#kernel_console_getchar).

Inputs: none.

Outputs: character.

Side effects: blocks on console input.

### sys_putchar

Definition: `logOS/main/components/kernel/syscalls/sys_console.c:12`.

Purpose: older console syscall wrapper around [kernel_console_putchar](#kernel_console_putchar).

Inputs: character.

Outputs: character written.

Side effects: writes to kernel console.

### early sys_write

Definition: `logOS/main/components/kernel/syscalls/sys_console.c:17`.

Purpose: older console-only write syscall implementation.

Inputs: fd, buffer, length.

Outputs: bytes written or `-1`.

Side effects: writes to kernel console for stdout/stderr only.

<a id="chapter-10-local-esp-idf-support"></a>

## [Chapter 10: Local ESP-IDF Support](#chapter-10-local-esp-idf-support)

Chapter Purpose: Summarizes the local ESP-IDF-derived `esp_system` component and the startup contrast relevant to logOS.

`logOS/components/esp_system` is ESP-IDF-derived local code. It supplies the ESP32-C3 platform services that remain linked around logOS: error helpers, startup support, panic/backtrace, reset/restart, IPC, watchdogs, system time, clock/cache/APB helpers, and public headers.

### Local esp_system Component Sources

| Area | Active source files in local component | Public/local entry points |
| --- | --- | --- |
| Error handling | `esp_err.c` | `_esp_error_check_failed_without_abort`, `_esp_error_check_failed` |
| Restart/system | `esp_system.c`, `port/esp_system_chip.c`, `port/soc/esp32c3/system_internal.c` | `esp_restart`, `esp_restart_noos`, `esp_restart_noos_dig`, `esp_get_free_heap_size`, `esp_get_free_internal_heap_size`, `esp_get_minimum_free_heap_size`, `esp_get_idf_version`, `esp_system_abort`, `esp_system_reset_modules_on_exit` |
| Startup | `startup.c`, `startup_funcs.c`, `port/cpu_start.c` | `start_cpu0`, `start_cpu_other_cores`, `esp_startup_start_app_other_cores`, `esp_system_include_startup_funcs`, `ESP_SYSTEM_INIT_FN(...)` startup hooks |
| Panic/debug | `panic.c`, `port/panic_handler.c`, `port/arch/riscv/panic_arch.c`, `port/arch/riscv/debug_helpers.c`, `port/arch/riscv/debug_stubs.c`, `eh_frame_parser.c`, `fp_unwind.c` | `panicHandler`, `xt_unhandled_exception`, `panic_restart`, `panic_abort`, `esp_backtrace_print`, `esp_backtrace_print_all_tasks`, `esp_fp_print_backtrace`, `esp_eh_frame_print_backtrace`, libunwind `unw_*` |
| IPC | `crosscore_int.c`, `esp_ipc.c`, `port/esp_ipc_isr.c`, `port/arch/riscv/esp_ipc_isr_*` | `esp_crosscore_int_*`, `esp_ipc_call`, `esp_ipc_call_blocking`, `esp_ipc_call_nonblocking`, `esp_ipc_isr_*` |
| Watchdogs | `int_wdt.c`, `xt_wdt.c`, `task_wdt/task_wdt.c`, `task_wdt/task_wdt_impl_timergroup.c` | `esp_int_wdt_init`, `esp_int_wdt_cpu_init`, `esp_task_wdt_*`, `esp_xt_wdt_*` |
| FreeRTOS hooks | `freertos_hooks.c` | `esp_register_freertos_idle_hook*`, `esp_register_freertos_tick_hook*`, deregistration functions, `esp_vApplicationTickHook`, `esp_vApplicationIdleHook` |
| ESP32-C3 hardware support | `port/soc/esp32c3/clk.c`, `reset_reason.c`, `cache_err_int.c`, `apb_backup_dma.c` | `esp_clk_init`, `esp_rtc_init`, `esp_perip_clk_init`, `esp_reset_reason`, `esp_cache_err_*`, `esp_apb_backup_dma_lock_init` |
| Image/flash mapping | `port/image_process.c` | `image_process`, `image_process_get_flash_segments_info` |
| Safety/runtime | `stack_check.c`, `ubsan.c`, `system_time.c`, `debug_assist.c` | `__stack_chk_fail`, UBSAN handlers, system time constructor hooks, hardware stack guard helpers |

### ESP-IDF-Derived Startup Contrast

Normal ESP-IDF reaches component initialization through `start_cpu0_default` and the linked startup function tables. logOS's intended custom startup path skips that walker and enters [kernel_start](#kernel_start). That has two practical consequences visible in this repository:

- Any ESP-IDF service normally initialized by an `ESP_SYSTEM_INIT_FN` may need explicit on-demand initialization before logOS uses it.
- [logos_partition_init](#logos_partition_init) explicitly initializes the default flash chip because the custom kernel path may not have run ESP-IDF's normal flash initialization startup function.

<a id="chapter-11-built-in-esp-idf-api-appendix"></a>

## [Chapter 11: Built-In ESP-IDF API Appendix](#chapter-11-built-in-esp-idf-api-appendix)

Chapter Purpose: Lists relevant ESP-IDF APIs and compiled component groups visible in the current build artifacts.

This appendix is limited to APIs from ESP-IDF components visible in the current build artifacts under `logOS/build/esp-idf` and relevant to this firmware. It is not a full ESP-IDF API manual.

### APIs called directly by logOS

| Component | API | Header | Purpose in logOS |
| --- | --- | --- | --- |
| `esp_rom` | `esp_rom_output_putc`, `esp_rom_output_rx_one_char`, `esp_rom_vprintf` | `esp-idf/components/esp_rom/include/esp_rom_serial_output.h`, `esp_rom_sys.h` | Kernel console output/input in [kernel_console_putchar](#kernel_console_putchar), [kernel_console_getchar](#kernel_console_getchar), [kernel_console_printf](#kernel_console_printf) |
| `esp_rom` | `esp_rom_delay_us` | `esp-idf/components/esp_rom/include/esp_rom_sys.h` | Busy delay in [kernel_delay_ms](#kernel_delay_ms) and startup code |
| `spi_flash` / `esp_flash` | `esp_flash_init_default_chip`, `esp_flash_read`, `esp_flash_write`, `esp_flash_erase_region` | `esp-idf/components/spi_flash/include/esp_flash.h`; local private `esp_private/esp_flash_internal.h` | Fixed partition access in [logos_partition_init](#logos_partition_init), [logos_partition_read](#logos_partition_read), [logos_partition_write](#logos_partition_write), [logos_partition_erase](#logos_partition_erase) |
| `esp_system` | `esp_restart_noos`, reset helpers | `logOS/components/esp_system/include/esp_system.h`, private headers | Used by startup and panic paths |
| `hal` / `soc` / `esp_rom` | UART low-level and interrupt-matrix helpers | `hal/uart_ll.h`, `soc/uart_pins.h`, `esp_rom_sys.h` | Console UART setup and interrupt clearing in custom startup |

### Enabled compiled component groups

The build tree contains compiled libraries for these notable ESP-IDF components: `app_update`, `bootloader_support`, `efuse`, `esp_app_format`, `esp_bootloader_format`, `esp_common`, `esp_driver_gpio`, `esp_driver_gptimer`, `esp_driver_i2c`, `esp_driver_i2s`, `esp_driver_spi`, `esp_driver_uart`, `esp_driver_usb_serial_jtag`, `esp_event`, `esp_hw_support`, `esp_libc`, `esp_mm`, `esp_netif`, `esp_partition`, `esp_phy`, `esp_pm`, `esp_rom`, `esp_security`, `esp_stdio`, `esp_system`, `esp_timer`, `esp_wifi`, `freertos`, `hal`, `heap`, `log`, `lwip`, `mbedtls`, `nvs_flash`, `pthread`, `riscv`, `soc`, `spi_flash`, `unity`, `vfs`, `wear_levelling`, and `wpa_supplicant`.

### esp_system

Headers:

- Local: `logOS/components/esp_system/include/esp_system.h`
- Local: `logOS/components/esp_system/include/esp_task_wdt.h`
- Local: `logOS/components/esp_system/include/esp_freertos_hooks.h`
- Local: `logOS/components/esp_system/include/esp_ipc.h`
- Local: `logOS/components/esp_system/include/esp_ipc_isr.h`
- Local: `logOS/components/esp_system/include/esp_debug_helpers.h`
- Local: `logOS/components/esp_system/include/esp_expression_with_stack.h`
- Local: `logOS/components/esp_system/include/esp_xt_wdt.h`

Representative public APIs:

- Restart and identity: `esp_restart`, `esp_register_shutdown_handler`, `esp_unregister_shutdown_handler`, `esp_get_free_heap_size`, `esp_get_free_internal_heap_size`, `esp_get_minimum_free_heap_size`, `esp_get_idf_version`.
- Reset reason: `esp_reset_reason`, `esp_reset_reason_set_hint`, `esp_reset_reason_get_hint`, `esp_reset_reason_clear_hint`.
- Task watchdog: `esp_task_wdt_init`, `esp_task_wdt_reconfigure`, `esp_task_wdt_deinit`, `esp_task_wdt_add`, `esp_task_wdt_add_user`, `esp_task_wdt_reset`, `esp_task_wdt_reset_user`, `esp_task_wdt_delete`, `esp_task_wdt_delete_user`, `esp_task_wdt_status`, `esp_task_wdt_print_triggered_tasks`.
- Interrupt watchdog: `esp_int_wdt_init`, `esp_int_wdt_cpu_init`, `esp_int_wdt_reconfigure_ticks`.
- XT WDT: `esp_xt_wdt_init`, `esp_xt_wdt_restore_clk`, `esp_xt_wdt_register_callback`.
- FreeRTOS hooks: `esp_register_freertos_idle_hook_for_cpu`, `esp_register_freertos_idle_hook`, `esp_register_freertos_tick_hook_for_cpu`, `esp_register_freertos_tick_hook`, matching deregistration functions.
- IPC: `esp_ipc_call`, `esp_ipc_call_blocking`, `esp_ipc_call_nonblocking`, `esp_ipc_isr_init`, `esp_ipc_isr_call`, `esp_ipc_isr_call_blocking`, `esp_ipc_isr_stall_other_cpu`, `esp_ipc_isr_release_other_cpu`.
- Debug/backtrace: `esp_backtrace_print`, `esp_backtrace_print_all_tasks`, `esp_execute_shared_stack_function`.

### spi_flash and esp_flash

Headers:

- `esp-idf/components/spi_flash/include/esp_flash.h`
- `esp-idf/components/spi_flash/include/esp_flash_err.h`
- `esp-idf/components/spi_flash/include/spi_flash_mmap.h`
- `esp-idf/components/spi_flash/include/esp_flash_spi_init.h`

Representative public APIs:

- Chip lifecycle and metadata: `esp_flash_init`, `esp_flash_init_default_chip`, `esp_flash_get_size`, `esp_flash_get_chip_write_protect`.
- I/O: `esp_flash_read`, `esp_flash_write`, `esp_flash_erase_region`, `esp_flash_erase_chip`.
- Memory mapping: `spi_flash_mmap`, `spi_flash_munmap`, `spi_flash_mmap_dump`, `spi_flash_cache2phys`, `spi_flash_phys2cache`.
- OS hooks/counters: `esp_flash_app_disable_os_functions`, `esp_flash_app_enable_os_functions`, flash counter APIs.

logOS calls the flash I/O APIs directly from [logos_partition_read](#logos_partition_read), [logos_partition_write](#logos_partition_write), and [logos_partition_erase](#logos_partition_erase).

### esp_rom

Headers:

- `esp-idf/components/esp_rom/include/esp_rom_sys.h`
- `esp-idf/components/esp_rom/include/esp_rom_serial_output.h`
- `esp-idf/components/esp_rom/include/esp_rom_spiflash.h`
- `esp-idf/components/esp_rom/include/esp_rom_uart.h`
- `esp-idf/components/esp_rom/include/esp_rom_gpio.h`

Representative APIs:

- Console/printing: `esp_rom_printf`, `esp_rom_vprintf`, `esp_rom_output_putc`, `esp_rom_output_tx_wait_idle`, `esp_rom_output_rx_one_char`.
- Delay/reset: `esp_rom_delay_us`, `esp_rom_software_reset_cpu`, `esp_rom_software_reset_system`, `esp_rom_get_reset_reason`.
- Low-level routing/flash helpers: `esp_rom_route_intr_matrix`, ROM SPI flash helpers.

### esp_driver_uart

Headers:

- `esp-idf/components/esp_driver_uart/include/driver/uart.h`
- `esp-idf/components/esp_driver_uart/include/driver/uart_vfs.h`

Representative APIs:

- Driver installation/configuration: `uart_driver_install`, `uart_driver_delete`, `uart_param_config`, `uart_set_pin`.
- Data path: `uart_read_bytes`, `uart_write_bytes`, `uart_flush`, `uart_wait_tx_done`.
- VFS integration: UART VFS registration functions.

logOS itself currently uses ROM console and low-level UART startup helpers rather than the public UART driver.

### esp_timer

Header: `esp-idf/components/esp_timer/include/esp_timer.h`.

Representative APIs:

- `esp_timer_create`, `esp_timer_delete`, `esp_timer_start_once`, `esp_timer_start_periodic`, `esp_timer_stop`, `esp_timer_get_time`, `esp_timer_dump`.

The local task WDT can use esp_timer in some configurations, but the current local `esp_system` CMake selects `task_wdt_impl_timergroup.c` unless `CONFIG_ESP_TASK_WDT_USE_ESP_TIMER` is enabled.

### freertos

Headers:

- `esp-idf/components/freertos/FreeRTOS-Kernel/include/freertos/FreeRTOS.h`
- `esp-idf/components/freertos/FreeRTOS-Kernel/include/freertos/task.h`
- `esp-idf/components/freertos/FreeRTOS-Kernel/include/freertos/semphr.h`
- ESP-IDF additions under `esp-idf/components/freertos/esp_additions/include`.

Representative APIs:

- Tasks: `xTaskCreate`, `xTaskCreatePinnedToCore`, `vTaskDelete`, `vTaskDelay`, `xTaskGetCurrentTaskHandle`, `xTaskGetSchedulerState`, `vTaskSuspendAll`, `xTaskResumeAll`.
- Synchronization: semaphore and queue APIs.
- ESP-IDF task inspection: task snapshots and core-affinity helpers used by panic/backtrace/watchdog code.

The intended logOS kernel path avoids the normal FreeRTOS `main_task` handoff when custom startup is linked.

### heap

Headers:

- `esp-idf/components/heap/include/esp_heap_caps.h`
- `esp-idf/components/heap/include/multi_heap.h`
- `esp-idf/components/heap/include/esp_heap_trace.h`

Representative APIs:

- Allocation: `heap_caps_malloc`, `heap_caps_calloc`, `heap_caps_realloc`, `heap_caps_free`.
- Introspection: `heap_caps_get_free_size`, `heap_caps_get_minimum_free_size`, `heap_caps_get_info`.
- Tracing/debug: heap trace APIs.

Local `esp_system` uses heap introspection for `esp_get_free_heap_size` style APIs.

### vfs and esp_stdio

Headers:

- `esp-idf/components/vfs/include/esp_vfs.h`
- `esp-idf/components/vfs/include/esp_vfs_dev.h`
- `esp-idf/components/vfs/include/esp_vfs_null.h`
- `esp-idf/components/esp_stdio/include/esp_stdio.h`
- `esp-idf/components/esp_stdio/include/esp_system_console.h`

Representative APIs:

- VFS registration: `esp_vfs_register`, `esp_vfs_unregister`, `esp_vfs_register_fd_range`, common VFS helpers.
- Device VFS: UART/USB serial JTAG/nullfs registration helpers.
- Standard I/O setup used by normal ESP-IDF startup.

logOS avoids stdio/VFS for kernel console output and uses ROM console functions directly.

### esp_partition and nvs_flash

Headers:

- `esp-idf/components/esp_partition/include/esp_partition.h`
- `esp-idf/components/nvs_flash/include/nvs_flash.h`
- `esp-idf/components/nvs_flash/include/nvs.h`

Representative APIs:

- Partition table: `esp_partition_find`, `esp_partition_find_first`, `esp_partition_read`, `esp_partition_write`, `esp_partition_erase_range`.
- NVS: `nvs_flash_init`, `nvs_open`, `nvs_get_*`, `nvs_set_*`, `nvs_commit`, `nvs_close`.

logOS currently uses a fixed offset/size for `logosfs` rather than querying `esp_partition`.

### esp_wifi and networking stack

Headers:

- `esp-idf/components/esp_wifi/include/esp_wifi.h`
- `esp-idf/components/esp_wifi/include/esp_wifi_types.h`
- `esp-idf/components/esp_netif/include/esp_netif.h`
- `esp-idf/components/esp_event/include/esp_event.h`

Representative APIs:

- Wi-Fi lifecycle/configuration: `esp_wifi_init`, `esp_wifi_set_mode`, `esp_wifi_set_config`, `esp_wifi_start`, `esp_wifi_stop`, `esp_wifi_connect`, `esp_wifi_disconnect`.
- Network interfaces: `esp_netif_init`, `esp_netif_create_default_wifi_sta`, `esp_netif_create_default_wifi_ap`.
- Events: `esp_event_loop_create_default`, `esp_event_handler_register`, `esp_event_post`.

`esp_wifi` is compiled into the current build tree, but logOS kernel code does not currently call Wi-Fi APIs directly.

### bootloader_support

Headers:

- `esp-idf/components/bootloader_support/include/bootloader_common.h`
- `esp-idf/components/bootloader_support/include/bootloader_utility.h`
- `esp-idf/components/bootloader_support/include/esp_image_format.h`
- `esp-idf/components/bootloader_support/include/esp_flash_partitions.h`

Representative APIs:

- Bootloader state and partition selection helpers.
- Image loading/verification helpers.
- Flash partition table structures and constants.

The custom bootloader [Bootloader call_start_cpu0](#bootloader-call_start_cpu0) uses these APIs to load the selected application.

<a id="chapter-12-verification-notes"></a>

## [Chapter 12: Verification Notes](#chapter-12-verification-notes)

Chapter Purpose: Records scope and maintenance notes for this technical map.

- This document intentionally avoids editing generated build outputs under `logOS/build`, `logOS/main/build`, and `.cache`.
- Paths and line numbers were taken from the current repository state.
- The built-in API appendix is scoped to components with compiled libraries in `logOS/build/esp-idf` and APIs relevant to ESP32-C3/logOS behavior.
