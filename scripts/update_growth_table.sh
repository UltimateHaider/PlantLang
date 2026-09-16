#!/bin/sh
# scripts/update_growth_table.sh — Update growth table in RELEASES.md
# Run: make update-growth

set -e

BASE="$(cd "$(dirname "$0")/.." && pwd)"
RELEASES="$BASE/docs/RELEASES.md"
MAKEFILE="$BASE/Makefile"

# Extract version from Makefile
VERSION=$(grep '^VERSION' "$MAKEFILE" | head -1 | sed 's/.*= *//')

# Get file sizes
RT_SIZE=$(wc -l < "$BASE/runtime/c/plant_runtime.c")
TENSOR_SIZE=$(wc -l < "$BASE/runtime/c/plant_tensor.c")
if [ -f "$BASE/runtime/c/plant_memory.c" ]; then
    MEM_SIZE=$(wc -l < "$BASE/runtime/c/plant_memory.c")
else
    MEM_SIZE="—"
fi
REPORT_SIZE=$(wc -l < "$BASE/runtime/c/plant_report.c")
MATH_SIZE=$(wc -l < "$BASE/runtime/c/plant_math.c")

# Get binary size
if [ -f "$BASE/bin/Chloroplast" ]; then
    BIN_SIZE=$(stat -c%s "$BASE/bin/Chloroplast" 2>/dev/null || echo "0")
else
    BIN_SIZE="TBD"
fi

# Find the line to replace (the first data row after the header)
# Pattern: | v0.51.1 | ... | — | ... |
LINE_NUM=$(grep -n "^| v0\." "$RELEASES" | head -1 | cut -d: -f1)

if [ -z "$LINE_NUM" ]; then
    echo "ERROR: Could not find version row in $RELEASES"
    exit 1
fi

# Build the new row
NEW_ROW="| v${VERSION} | ${BIN_SIZE} | ${RT_SIZE} | ${TENSOR_SIZE} | ${MEM_SIZE} | ${REPORT_SIZE} | ${MATH_SIZE} |"

# Replace the line
sed -i "${LINE_NUM}s/.*/${NEW_ROW}/" "$RELEASES"

# Update the auto-updated comment
sed -i "s/# Auto-updated: .*/# Auto-updated: ${VERSION}/" "$RELEASES"

echo "RELEASES.md updated for v${VERSION}"
echo "  Binary: ${BIN_SIZE} bytes"
echo "  plant_runtime.c: ${RT_SIZE} lines"
echo "  plant_tensor.c: ${TENSOR_SIZE} lines"
echo "  plant_memory.c: ${MEM_SIZE} lines"
echo "  plant_report.c: ${REPORT_SIZE} lines"
echo "  plant_math.c: ${MATH_SIZE} lines"
