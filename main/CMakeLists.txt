set(USER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/components/user")
set(USER_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/user")
set(USER_SHELL_ELF "${USER_BUILD_DIR}/shell.elf")
set(USER_COMMON_FLAGS
    -O
    -g
    -ffreestanding
    -fPIE
    -fno-jump-tables
    -march=rv32im_zicsr
    -mabi=ilp32
    -mcmodel=medany
    "-I${USER_SOURCE_DIR}"
    "-I${CMAKE_CURRENT_LIST_DIR}/components/kernel"
    "-I${CMAKE_CURRENT_LIST_DIR}/components/kernel/esp32c3"
)

idf_component_register(
    SRCS
        "components/kernel/esp32c3/startup/start_cpu0.c"
        "components/kernel/main.c"
        "components/kernel/loader.c"
        "components/kernel/esp32c3/block.c"
        "components/kernel/esp32c3/console.c"
        "components/kernel/device.c"
        "components/kernel/dir.c"
        "components/kernel/file.c"
        "components/kernel/fs_alloc.c"
        "components/kernel/fs.c"
        "components/kernel/process.c"
        "components/kernel/pipe.c"
        "components/kernel/inode.c"
        "components/kernel/syscall.c"
        "components/kernel/kernel_time.c"
        "components/kernel/esp32c3/timer.c"
        "components/kernel/esp32c3/logos_partition.c"
        "components/kernel/esp32c3/trap.S"
        "components/kernel/esp32c3/console_dev.c"
        "components/kernel/esp32c3/sys_console.c"
        "components/kernel/esp32c3/seed_filesystem.c"
        #"components/kernel/logos_string.c"
        #"components/kernel/logos_string.h"
    INCLUDE_DIRS
        "."
        "components/kernel"
        "components/kernel/esp32c3"
        "components/kernel/esp32c3/startup"
    WHOLE_ARCHIVE
)

add_custom_command(
    OUTPUT "${USER_SHELL_ELF}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${USER_BUILD_DIR}"
    COMMAND "${CMAKE_C_COMPILER}" ${USER_COMMON_FLAGS}
            -c "${USER_SOURCE_DIR}/user_crt0.S" -o "${USER_BUILD_DIR}/user_crt0.o"
    COMMAND "${CMAKE_C_COMPILER}" ${USER_COMMON_FLAGS}
            -c "${USER_SOURCE_DIR}/syscall.S" -o "${USER_BUILD_DIR}/syscall.o"
    COMMAND "${CMAKE_C_COMPILER}" ${USER_COMMON_FLAGS}
            -c "${USER_SOURCE_DIR}/libc.c" -o "${USER_BUILD_DIR}/libc.o"
    COMMAND "${CMAKE_C_COMPILER}" ${USER_COMMON_FLAGS}
            -c "${USER_SOURCE_DIR}/shell.c" -o "${USER_BUILD_DIR}/shell.o"
    COMMAND "${CMAKE_LINKER}" -nostdlib
            -T "${USER_SOURCE_DIR}/user.lds"
            -o "${USER_BUILD_DIR}/shell-debug.elf"
            "${USER_BUILD_DIR}/user_crt0.o"
            "${USER_BUILD_DIR}/syscall.o"
            "${USER_BUILD_DIR}/libc.o"
            "${USER_BUILD_DIR}/shell.o"
    COMMAND "${CMAKE_STRIP}" -s "${USER_BUILD_DIR}/shell-debug.elf"
            -o "${USER_SHELL_ELF}"
    DEPENDS
        "${USER_SOURCE_DIR}/user_crt0.S"
        "${USER_SOURCE_DIR}/syscall.S"
        "${USER_SOURCE_DIR}/libc.c"
        "${USER_SOURCE_DIR}/libc.h"
        "${USER_SOURCE_DIR}/shell.c"
        "${USER_SOURCE_DIR}/user.lds"
        "${CMAKE_CURRENT_LIST_DIR}/components/kernel/syscall_nr.h"
    COMMENT "Building embedded /bin/sh userspace ELF"
    VERBATIM
)

add_custom_target(user_shell DEPENDS "${USER_SHELL_ELF}")

target_add_binary_data(
    ${COMPONENT_LIB}
    "${USER_SHELL_ELF}"
    BINARY
    RENAME_TO logos_shell_elf
    DEPENDS user_shell
)

function(add_logos_user_program program_name)
    set(program_elf "${USER_BUILD_DIR}/${program_name}.elf")
    set(program_object "${USER_BUILD_DIR}/${program_name}.o")
    set(program_target "user_${program_name}")

    add_custom_command(
        OUTPUT "${program_elf}"
        COMMAND "${CMAKE_C_COMPILER}" ${USER_COMMON_FLAGS}
                -c "${USER_SOURCE_DIR}/${program_name}.c" -o "${program_object}"
        COMMAND "${CMAKE_LINKER}" -nostdlib
                -T "${USER_SOURCE_DIR}/user.lds"
                -o "${USER_BUILD_DIR}/${program_name}-debug.elf"
                "${USER_BUILD_DIR}/user_crt0.o"
                "${USER_BUILD_DIR}/syscall.o"
                "${USER_BUILD_DIR}/libc.o"
                "${program_object}"
        COMMAND "${CMAKE_STRIP}" -s "${USER_BUILD_DIR}/${program_name}-debug.elf"
                -o "${program_elf}"
        DEPENDS
            user_shell
            "${USER_SOURCE_DIR}/${program_name}.c"
            "${USER_SOURCE_DIR}/libc.h"
            "${USER_SOURCE_DIR}/user.lds"
        COMMENT "Building embedded /bin/${program_name} userspace ELF"
        VERBATIM
    )

    add_custom_target(${program_target} DEPENDS "${program_elf}")
    target_add_binary_data(
        ${COMPONENT_LIB}
        "${program_elf}"
        BINARY
        RENAME_TO "logos_${program_name}_elf"
        DEPENDS ${program_target}
    )
endfunction()

foreach(program_name
        hello cat cp ed env_demo fd_test fork_demo kill ln ls mkdir mknod mv
        pipe_demo pipe_test ps redir_test rm rmdir spawn_demo)
    add_logos_user_program(${program_name})
endforeach()

target_compile_options(
      ${COMPONENT_LIB}
      PRIVATE
      -Wno-error=builtin-declaration-mismatch
      -Wno-error=format
  )

# Extend ESP-IDF's generated memory layout with the logOS IRAM trap section.
target_linker_script(
    ${COMPONENT_LIB}
    INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/components/kernel/esp32c3/trap.lds"
)
