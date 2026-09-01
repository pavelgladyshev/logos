#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_EXPORT="$PROJECT_DIR/../esp-idf/export.sh"

if [[ ! -f "$IDF_EXPORT" ]]; then
    echo "ESP-IDF was not found at $IDF_EXPORT" >&2
    echo "Place the ESP-IDF checkout beside logos, or update IDF_EXPORT in scripts/idf.sh." >&2
    exit 1
fi

# export.sh selects ESP-IDF's Python environment and puts its tools first in PATH.
source "$IDF_EXPORT" >/dev/null
cd "$PROJECT_DIR"
exec idf.py "$@"
