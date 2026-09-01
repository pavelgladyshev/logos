#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# idf.py builds the firmware, assembles the flash/eFuse images, and supplies
# the target-specific devices and watchdog settings expected by ESP32-C3 QEMU.
exec "$PROJECT_DIR/scripts/idf.sh" -B build qemu "$@"
