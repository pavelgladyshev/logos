#!/bin/bash
#
# check_pie.sh - Check if a RISC-V ELF file is position-independent
# Licensed under Creative Commons Attribution International License 4.0
#
# Analyzes object files and executables for non-PIE patterns that will
# break when loaded at a different address than the link address.
#
# Usage: ./check_pie.sh <file.o or file.elf> [--verbose]
#

set -e

# Find the RISC-V toolchain prefix
if command -v riscv64-elf-readelf &> /dev/null; then
    READELF="riscv64-elf-readelf"
    OBJDUMP="riscv64-elf-objdump"
elif command -v /opt/homebrew/bin/riscv64-elf-readelf &> /dev/null; then
    READELF="/opt/homebrew/bin/riscv64-elf-readelf"
    OBJDUMP="/opt/homebrew/bin/riscv64-elf-objdump"
elif command -v riscv64-unknown-elf-readelf &> /dev/null; then
    READELF="riscv64-unknown-elf-readelf"
    OBJDUMP="riscv64-unknown-elf-objdump"
elif command -v riscv64-linux-elf-readelf &> /dev/null; then
    READELF="riscv64-linux-elf-readelf"
    OBJDUMP="riscv64-linux-elf-objdump"
else
    echo "Error: Cannot find riscv64 elf readelf" >&2
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 <file.o or file.elf> [--verbose]"
    echo ""
    echo "Checks if a RISC-V ELF file contains position-independent code."
    echo "Returns 0 if PIE-safe, 1 if problems found."
    exit 1
fi

FILE="$1"
VERBOSE=""
if [ "$2" = "--verbose" ] || [ "$2" = "-v" ]; then
    VERBOSE=1
fi

if [ ! -f "$FILE" ]; then
    echo "Error: File not found: $FILE" >&2
    exit 1
fi

ERRORS=0
WARNINGS=0

echo "=== PIE Check: $FILE ==="
echo ""

# Determine if this is an object file or executable
FILE_TYPE=$($READELF -h "$FILE" 2>/dev/null | grep "Type:" | awk '{print $2}')

case "$FILE_TYPE" in
    REL)
        echo "File type: Relocatable object file"
        IS_OBJECT=1
        ;;
    EXEC)
        echo "File type: Executable (ET_EXEC)"
        IS_OBJECT=0
        ;;
    DYN)
        echo "File type: Shared object / PIE (ET_DYN)"
        IS_OBJECT=0
        ;;
    *)
        echo "File type: Unknown ($FILE_TYPE)"
        IS_OBJECT=0
        ;;
esac
echo ""

#
# Check 1: Look for problematic relocations in object files
#
if [ "$IS_OBJECT" = "1" ]; then
    echo "--- Checking relocations ---"

    # R_RISCV_32 in data sections = absolute address that won't be relocated
    BAD_RELOCS=$($READELF -r "$FILE" 2>/dev/null | grep -E "\.rela\.data|\.rela\.sdata" | head -1)

    if [ -n "$BAD_RELOCS" ]; then
        # Check for R_RISCV_32 relocations (absolute 32-bit addresses)
        R32_COUNT=$($READELF -r "$FILE" 2>/dev/null | grep "R_RISCV_32" | wc -l | tr -d ' ')

        if [ "$R32_COUNT" -gt 0 ]; then
            echo "ERROR: Found $R32_COUNT R_RISCV_32 relocations in data sections"
            echo "       These are absolute addresses that won't work when relocated."
            echo ""
            if [ -n "$VERBOSE" ]; then
                echo "Problematic relocations:"
                $READELF -r "$FILE" 2>/dev/null | grep -B2 "R_RISCV_32" | head -20
                echo ""
            fi
            echo "       Likely cause: Static initialization of pointer arrays"
            echo "       Fix: Use runtime initialization instead"
            echo ""
            ERRORS=$((ERRORS + 1))
        fi
    fi

    # Check for .data.rel.local section (indicates static pointer initialization)
    if $READELF -r "$FILE" 2>/dev/null | grep -q "\.rela\.data\.rel\.local"; then
        echo "WARNING: Found .rela.data.rel.local section"
        echo "         This indicates static pointer array initialization."
        echo ""
        if [ -n "$VERBOSE" ]; then
            echo "Relocations in .data.rel.local:"
            $READELF -r "$FILE" 2>/dev/null | grep -A100 "\.rela\.data\.rel\.local" | grep "R_RISCV" | head -10
            echo ""
        fi
        WARNINGS=$((WARNINGS + 1))
    fi

    # Check .rela.text for non-PC-relative relocations
    # Extract only the .rela.text section (stop at the next "Relocation section" line)
    NON_PCREL=$($READELF -r "$FILE" 2>/dev/null | \
        sed -n "/^Relocation section '\.rela\.text'/,/^Relocation section/p" | \
        grep "R_RISCV" | \
        grep -v "PCREL\|CALL\|RELAX\|BRANCH\|JAL\|RVC_" | \
        head -10)

    if [ -n "$NON_PCREL" ]; then
        echo "WARNING: Found non-PC-relative relocations in .text section:"
        echo "$NON_PCREL"
        echo ""
        WARNINGS=$((WARNINGS + 1))
    fi

    echo "Relocation check complete."
    echo ""
fi

#
# Check 2: Look for absolute addresses in the code (for linked executables)
#
if [ "$IS_OBJECT" = "0" ]; then
    echo "--- Checking for absolute address patterns ---"

    # Look for 'lui' followed by operations that might be loading absolute addresses
    # In PIE code, addresses should use auipc, not lui for address computation

    # Get the linked address range
    LOAD_ADDR=$($READELF -l "$FILE" 2>/dev/null | grep "LOAD" | head -1 | awk '{print $3}')

    if [ -n "$LOAD_ADDR" ]; then
        echo "Link address: $LOAD_ADDR"

        # Check if link address is 0 (expected for PIE)
        if [ "$LOAD_ADDR" = "0x00000000" ] || [ "$LOAD_ADDR" = "0x0" ]; then
            echo "Good: Linked at address 0 (relocatable)"
        else
            echo "WARNING: Linked at non-zero address"
            echo "         Program may not work if loaded elsewhere"
            WARNINGS=$((WARNINGS + 1))
        fi
    fi
    echo ""

    # Check for .dynamic section (indicates proper PIE with relocations)
    if $READELF -S "$FILE" 2>/dev/null | grep -q "\.dynamic"; then
        echo "Good: Has .dynamic section (proper PIE)"
    else
        echo "Note: No .dynamic section"
        echo "      Relocations were resolved at link time."
        echo "      Static pointer arrays will have wrong addresses if relocated."
    fi
    echo ""

    # Check for any remaining relocations
    RELOC_COUNT=$($READELF -r "$FILE" 2>/dev/null | grep "R_RISCV" | wc -l | tr -d ' ')
    if [ "$RELOC_COUNT" -gt 0 ]; then
        echo "Good: Has $RELOC_COUNT runtime relocations"
    else
        echo "Note: No runtime relocations in final executable"
    fi
    echo ""
fi

#
# Check 3: Look for data sections with embedded pointers
#
echo "--- Checking data sections ---"

# Get section info
$READELF -S "$FILE" 2>/dev/null | grep -E "\.data|\.sdata|\.rodata" | while read line; do
    SECT_NAME=$(echo "$line" | awk '{print $2}')
    SECT_SIZE=$(echo "$line" | awk '{print $6}')

    # Convert hex size to decimal for comparison
    if [ -n "$SECT_SIZE" ] && [ "$SECT_SIZE" != "00" ]; then
        echo "Section $SECT_NAME: size 0x$SECT_SIZE"
    fi
done

echo ""

#
# Check 4: Scan for common problematic patterns in source
#
echo "--- Summary ---"

if [ $ERRORS -gt 0 ]; then
    echo ""
    echo "RESULT: FAILED - $ERRORS error(s), $WARNINGS warning(s)"
    echo ""
    echo "The file contains non-position-independent code patterns."
    echo "It will likely fail when loaded at a different address."
    echo ""
    echo "Common fixes:"
    echo "  1. Avoid: char *arr[] = {\"str1\", \"str2\"};"
    echo "  2. Use:   char *arr[2]; arr[0] = \"str1\"; arr[1] = \"str2\";"
    echo "  3. Compile with: -fPIE -fno-jump-tables -mcmodel=medany"
    exit 1
elif [ $WARNINGS -gt 0 ]; then
    echo ""
    echo "RESULT: PASSED with $WARNINGS warning(s)"
    echo ""
    echo "The file appears mostly position-independent but has some concerns."
    exit 0
else
    echo ""
    echo "RESULT: PASSED"
    echo ""
    echo "No obvious position-independence issues detected."
    exit 0
fi
