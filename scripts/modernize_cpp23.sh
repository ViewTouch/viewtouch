#!/bin/bash
# modernize_cpp23.sh - Batch modernization script for ViewTouch C++23
#
# This script helps migrate ViewTouch codebase to C++23 patterns.
# Usage: ./modernize_cpp23.sh [--check|--apply]

set -euo pipefail

REPO_ROOT="/home/ariel/Documents/viewtouchFork"
cd "$REPO_ROOT"

echo "====================================================================="
echo "ViewTouch C++23 Modernization Script"
echo "====================================================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to check if cpp23_utils.hh is included
check_cpp23_include() {
    local file="$1"
    if grep -q '#include "src/utils/cpp23_utils.hh"' "$file" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Function to add cpp23_utils include to a file
add_cpp23_include() {
    local file="$1"
    local temp_file="${file}.tmp"
    
    # Find the last #include line and add our include after it
    awk '
        /#include/ { last_include=NR; line[NR]=$0; next }
        { line[NR]=$0 }
        END {
            for (i=1; i<=NR; i++) {
                print line[i]
                if (i==last_include) {
                    print "#include \"src/utils/cpp23_utils.hh\""
                }
            }
        }
    ' "$file" > "$temp_file"
    
    mv "$temp_file" "$file"
    echo -e "${GREEN}✓${NC} Added cpp23_utils.hh include to $(basename $file)"
}

# Scan for files needing modernization
echo "Scanning for files with sprintf/snprintf..."
FILES_WITH_SPRINTF=$(find zone main/business main/data main/hardware main/ui -name "*.cc" -exec grep -l "snprintf\|sprintf" {} \; 2>/dev/null || true)
TOTAL_FILES=$(echo "$FILES_WITH_SPRINTF" | wc -w)

echo "Found $TOTAL_FILES files with sprintf/snprintf calls"
echo ""

# Count total occurrences
TOTAL_SNPRINTF=$(grep -r "snprintf" zone main 2>/dev/null | wc -l || echo "0")
TOTAL_SPRINTF=$(grep -r "sprintf[^_]" zone main 2>/dev/null | wc -l || echo "0")

echo "Statistics:"
echo "  - snprintf calls: $TOTAL_SNPRINTF"
echo "  - sprintf calls: $TOTAL_SPRINTF"
echo "  - Total calls to modernize: $((TOTAL_SNPRINTF + TOTAL_SPRINTF))"
echo ""

# Check mode
if [[ "${1:-}" == "--check" ]]; then
    echo -e "${YELLOW}CHECK MODE${NC} - No changes will be made"
    echo ""
    echo "Files needing modernization:"
    for file in $FILES_WITH_SPRINTF; do
        count=$(grep -c "snprintf\|sprintf" "$file" 2>/dev/null || echo "0")
        has_include=""
        if check_cpp23_include "$file"; then
            has_include="${GREEN}[has cpp23_utils]${NC}"
        else
            has_include="${RED}[needs cpp23_utils]${NC}"
        fi
        echo -e "  $file: $count calls $has_include"
    done
    exit 0
fi

# Apply mode
if [[ "${1:-}" == "--apply" ]]; then
    echo -e "${GREEN}APPLY MODE${NC} - Adding cpp23_utils.hh includes"
    echo ""
    
    for file in $FILES_WITH_SPRINTF; do
        if ! check_cpp23_include "$file"; then
            add_cpp23_include "$file"
        else
            echo -e "${YELLOW}⊙${NC} $(basename $file) already has cpp23_utils.hh"
        fi
    done
    
    echo ""
    echo -e "${GREEN}✓ Complete!${NC} Added includes to files"
    echo ""
    echo "Next steps:"
    echo "  1. Review the added includes"
    echo "  2. Run manual conversion of snprintf → format_to_buffer"
    echo "  3. Build and test: cd build && cmake --build ."
    exit 0
fi

# Default: show help
echo "Usage: $0 [--check|--apply]"
echo ""
echo "Options:"
echo "  --check    Scan and report files needing modernization"
echo "  --apply    Add cpp23_utils.hh includes to files"
echo ""
echo "Manual conversion still required for snprintf → format_to_buffer"
