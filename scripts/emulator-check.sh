#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_EXPORT="$PROJECT_DIR/../esp-idf/export.sh"

if [[ ! -f "$IDF_EXPORT" ]]; then
    echo "ERROR: ESP-IDF is missing: $IDF_EXPORT" >&2
    exit 1
fi

source "$IDF_EXPORT" >/dev/null

for command in idf.py qemu-system-riscv32 riscv32-esp-elf-gcc; do
    if ! command -v "$command" >/dev/null; then
        echo "ERROR: required command is missing after ESP-IDF activation: $command" >&2
        exit 1
    fi
done

if ! qemu-system-riscv32 -machine help | grep -q 'esp32c3'; then
    echo "ERROR: qemu-system-riscv32 does not contain Espressif's ESP32-C3 machine." >&2
    echo "Install it with: python3 ../esp-idf/tools/idf_tools.py install qemu-riscv32" >&2
    exit 1
fi

echo "ESP32-C3 emulator environment is ready."
echo "  ESP-IDF: $(idf.py --version)"
echo "  QEMU:    $(qemu-system-riscv32 --version | head -n 1)"
echo "  GCC:     $(riscv32-esp-elf-gcc --version | head -n 1)"
