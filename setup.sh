#!/usr/bin/env bash
# setup.sh - get this computer ready to build and run LogOS, and check that it is
#
#   ./setup.sh           install what is missing, fetch Logisim, build LogOS, check
#   ./setup.sh --check   only check: installs, fetches, builds and changes nothing
#
# For macOS (with Homebrew), Ubuntu or Debian, and Windows (Ubuntu on WSL 2). Each step says what
# it does, every command that changes something is shown, and the summary at the end lists what
# is not right. When you ask for help, paste all of the output.

cd "$(dirname "$0")" || exit 1
check_only=false
case "${1:-}" in
    --check) check_only=true ;;
    "") ;;
    *) echo "usage: ./setup.sh [--check]"; exit 2 ;;
esac

problems=""
step() { printf '\n== %s\n' "$*"; }
ok()   { printf '   ok      %s\n' "$*"; }
note() { printf '   note    %s\n' "$*"; }
bad()  { printf '   NOT OK  %s\n' "$*"; problems="$problems
   - $*"; }
show() { printf '   $ %s\n' "$*"; "$@"; }            # run a command, showing it first

# ---------------------------------------------------------------------------------------------
step "This computer"
platform=other
apt=false
if [ "$(uname -s)" = Darwin ]; then
    platform=macos
    ok "macOS $(sw_vers -productVersion), $(uname -m)"
elif [ "$(uname -s)" = Linux ]; then
    [ -r /etc/os-release ] && . /etc/os-release
    platform=linux
    if grep -qi microsoft /proc/version 2>/dev/null; then
        if [ -e /dev/fuse ]; then
            platform=wsl
            ok "Windows: ${PRETTY_NAME:-Linux} on WSL 2${WSL_DISTRO_NAME:+ (distribution $WSL_DISTRO_NAME)}"
        else
            bad "this is WSL 1, which has no FUSE: in PowerShell, wsl -l -v names the distribution, wsl --set-version <name> 2 converts it"
        fi
    else
        ok "${PRETTY_NAME:-Linux}, $(uname -m)"
    fi
    case " ${ID:-} ${ID_LIKE:-} " in
        *" debian "*|*" ubuntu "*) apt=true ;;
    esac
else
    bad "$(uname -s) is neither macOS nor Linux; on Windows, run ./setup.sh in Ubuntu on WSL 2 (README.md)"
fi
ok "LogOS $(git rev-parse --abbrev-ref HEAD 2>/dev/null) $(git log -1 --format='%h, %cd' --date=short 2>/dev/null), in $(pwd)"

# ---------------------------------------------------------------------------------------------
if [ "$platform" = linux ] || [ "$platform" = wsl ]; then
    step "Packages"
    pkgs="build-essential git curl pkg-config fuse3 libfuse3-dev gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf openjdk-21-jre"
    if ! $apt; then
        note "not Ubuntu or Debian: install your system's packages for these yourself: $pkgs"
    else
        missing=""
        for p in $pkgs; do
            dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q "install ok installed" || missing="$missing $p"
        done
        if [ -z "$missing" ]; then
            ok "installed: $pkgs"
        elif $check_only; then
            bad "not installed:$missing"
        else
            note "installing:$missing (sudo asks for your Linux password)"
            { show sudo apt-get update && show sudo apt-get install -y $missing; } || bad "apt-get could not install:$missing"
        fi
    fi
fi

if [ "$platform" = macos ]; then
    step "Homebrew packages"
    if ! command -v brew >/dev/null 2>&1; then
        for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do
            [ -x "$b" ] && eval "$("$b" shellenv)" && break
        done
    fi
    if ! command -v brew >/dev/null 2>&1; then
        bad "Homebrew is not installed: install it (https://brew.sh), then run ./setup.sh again"
    elif brew list --formula riscv64-elf-gcc >/dev/null 2>&1; then
        ok "riscv64-elf-gcc"
    elif $check_only; then
        bad "the RISC-V compiler is not installed (brew install riscv64-elf-gcc)"
    else
        show brew install riscv64-elf-gcc || bad "brew install riscv64-elf-gcc failed"
    fi
fi

# ---------------------------------------------------------------------------------------------
step "Java, 21 or later (for Logisim)"
java_major() { java -version 2>&1 | sed -n 's/.*version "\([0-9]*\)[."].*/\1/p' | head -1; }
v=$(java_major)
if [ -z "$v" ] || [ "$v" -lt 21 ]; then
    if [ "$platform" = macos ] && ! $check_only && command -v brew >/dev/null 2>&1; then
        note "installing Java 21 (its installer asks for your Mac's password)"
        show brew install --cask temurin@21
        v=$(java_major)
    fi
fi
if [ -n "$v" ] && [ "$v" -ge 21 ]; then
    ok "$(java -version 2>&1 | head -1)"
else
    bad "no Java 21 or later (java -version says: $(java -version 2>&1 | head -1))"
fi

# ---------------------------------------------------------------------------------------------
if [ "$platform" = wsl ]; then
    step "Windows"
    if grep -q '^user_allow_other' /etc/fuse.conf 2>/dev/null; then
        ok "user_allow_other in /etc/fuse.conf: File Explorer can open a disk mounted with -o allow_other"
    elif $check_only; then
        bad "user_allow_other is not in /etc/fuse.conf: File Explorer cannot open a mounted disk"
    else
        note "letting logosfs mount with -o allow_other, so that File Explorer can open a mounted disk"
        if grep -q '^#user_allow_other' /etc/fuse.conf 2>/dev/null; then
            show sudo sed -i 's/^#user_allow_other/user_allow_other/' /etc/fuse.conf
        else
            printf '   $ %s\n' "echo user_allow_other | sudo tee -a /etc/fuse.conf"
            echo user_allow_other | sudo tee -a /etc/fuse.conf >/dev/null
        fi || bad "could not change /etc/fuse.conf"
    fi
    if command -v javaw.exe >/dev/null 2>&1; then
        ok "Java for Windows is on the PATH: make gui opens Logisim in a native window ($(command -v javaw.exe))"
    elif [ -d /mnt/wslg ]; then
        ok "WSLg: make gui opens Logisim's window through it (Java for Windows on the PATH would give a native window and the FPGA board's USB)"
    else
        note "no WSLg and no Java for Windows on the PATH: make gui cannot open a window (make run, the text mode, works)"
    fi
    case "$(pwd)" in
        /mnt/*) note "this clone is on a Windows drive: LogOS builds much faster in Ubuntu's own folders, e.g. ~/logos" ;;
    esac
fi

# ---------------------------------------------------------------------------------------------
step "RISC-V compiler"
if ! command -v make >/dev/null 2>&1; then
    bad "make is not installed"
else
    prefix=$(make -s print-RISCV_TOOL_PREFIX)
    if "${prefix}gcc" --version >/dev/null 2>&1; then
        ok "${prefix}gcc: $("${prefix}gcc" --version | head -1)"
    else
        bad "${prefix}gcc not found"
    fi
fi

# ---------------------------------------------------------------------------------------------
step "Logisim Evolution UCD"
jar=$(make -s print-LOGISIM_JAR 2>/dev/null)
if [ -f "$jar" ]; then
    ok "$jar"
elif $check_only; then
    bad "$jar is not here (make logisim fetches it)"
elif show make -s logisim; then
    ok "$jar"
else
    bad "could not fetch $jar (make logisim)"
fi

# ---------------------------------------------------------------------------------------------
step "Building LogOS"
build_dir=$(make -s print-BUILD_DIR 2>/dev/null)
kernel="${build_dir:+$build_dir/}kernel.bin"         # 1.x builds in the top folder, not build/
if $check_only; then
    for f in boot-rom.txt "$kernel" fstool/fstool block_storage.bin; do
        if [ -f "$f" ]; then ok "$f"; else bad "$f is not built (./setup.sh builds it, or make all)"; fi
    done
else
    mkdir -p build
    if make -s boot-rom.txt "$kernel" user-programs fstool/fstool > build/setup-make.log 2>&1; then
        ok "bootloader, kernel, programs and fstool (the build's output: build/setup-make.log)"
    else
        tail -15 build/setup-make.log | sed 's/^/   | /'
        bad "the build failed (all its output: build/setup-make.log)"
    fi
    if [ -f block_storage.bin ]; then
        ok "block_storage.bin is here, and kept (make fs-image makes a fresh one)"
    elif make -s fs-image >> build/setup-make.log 2>&1; then
        ok "block_storage.bin made"
    else
        bad "make fs-image failed (build/setup-make.log)"
    fi
fi

step "The disk image"
if [ -f block_storage.bin ] && [ -x fstool/fstool ]; then
    if out=$(./fstool/fstool ls block_storage.bin / 2>&1); then
        ok "block_storage.bin has a LogOS file system, / holds: $(echo "$out" | awk 'NR > 1 && $NF != "." && $NF != ".." {print $NF}' | tr '\n' ' ')"
    else
        bad "block_storage.bin: $out"
    fi
else
    bad "cannot check the disk image: needs block_storage.bin and fstool/fstool"
fi

# ---------------------------------------------------------------------------------------------
step "LogOS boots in Logisim's text mode (on a copy of the disk image)"
if [ -f "$jar" ] && [ -f block_storage.bin ] && [ -n "$v" ] && [ "$v" -ge 21 ]; then
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/logos-setup.XXXXXX")
    cp computer.circ block_storage.bin "$tmp/"
    here=$(pwd)
    ( cd "$tmp" && exec java -Djava.awt.headless=true -jar "$here/$jar" computer.circ --tty raw ) \
        < /dev/null > "$tmp/console.txt" 2>&1 &
    pid=$!
    start=$(date +%s)
    while ! grep -q '\$ ' "$tmp/console.txt" 2>/dev/null; do
        kill -0 "$pid" 2>/dev/null || break
        [ $(( $(date +%s) - start )) -ge 120 ] && break
        sleep 1
    done
    secs=$(( $(date +%s) - start ))
    if grep -q '\$ ' "$tmp/console.txt"; then
        ok "the shell's prompt came after $secs s"
    else
        tail -15 "$tmp/console.txt" | sed 's/^/   | /'
        bad "no shell prompt after $secs s"
    fi
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
    rm -rf "${tmp:?}"
else
    bad "cannot try booting LogOS: needs Java 21, $jar and block_storage.bin"
fi

# ---------------------------------------------------------------------------------------------
step "logosfs (optional: mounts block_storage.bin as a folder of this computer)"
fuse=false
if [ "$platform" = macos ]; then
    [ -f /usr/local/include/fuse3/fuse.h ] && fuse=true
else
    [ -f /usr/include/fuse3/fuse.h ] && fuse=true
fi
if [ ! -f fstool/logosfs.c ]; then
    note "this LogOS has no logosfs"
elif ! $fuse; then
    note "no FUSE (on macOS: macFUSE, see README.md, Mounting a disk image on your computer)"
elif $check_only; then
    if [ -x fstool/logosfs ]; then ok "fstool/logosfs"; else note "fstool/logosfs is not built (make logosfs)"; fi
elif make -s logosfs >> build/setup-make.log 2>&1; then
    ok "fstool/logosfs built"
else
    bad "make logosfs failed (build/setup-make.log)"
fi
if [ "$platform" != macos ] && [ -x fstool/logosfs ] && [ -f block_storage.bin ]; then
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/logos-setup.XXXXXX")
    cp block_storage.bin "$tmp/disk.bin"
    mkdir "$tmp/mnt"
    if ./fstool/logosfs "$tmp/disk.bin" "$tmp/mnt" 2> "$tmp/error.txt" && [ -d "$tmp/mnt/bin" ]; then
        ok "it mounts a copy of the disk: $(ls "$tmp/mnt" | tr '\n' ' ')"
    else
        bad "logosfs could not mount a copy of the disk: $(cat "$tmp/error.txt")"
    fi
    fusermount3 -u "$tmp/mnt" 2>/dev/null
    mountpoint -q "$tmp/mnt" 2>/dev/null || rm -rf "${tmp:?}"
fi

# ---------------------------------------------------------------------------------------------
step "Summary"
if [ -n "$problems" ]; then
    printf '   Not ready yet:%s\n\n' "$problems"
    echo "   Fix these and run ./setup.sh again. When you ask for help, paste all of this output."
    exit 1
fi
echo "   Ready: make run starts LogOS in Logisim's text mode (Ctrl-C stops it), make gui opens the"
echo "   circuit in Logisim's window. README.md has the rest."
if [ "$platform" = wsl ]; then
    printf '   Windows programs find this folder at %s\n' \
        "\\\\wsl.localhost\\${WSL_DISTRO_NAME:-<distribution>}$(pwd | tr / '\\')"
fi
