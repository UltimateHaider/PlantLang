#!/bin/sh
# Run 6D tensor benchmark and record result.
# Usage: sh tests/perf/run_6d_bench.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_6d_bench
rm -rf "$BUILD"
mkdir -p "$BUILD"

echo "== 6D tensor benchmark =="
if ! "$PLANTC" "$DIR/tensor_6d_bench.plant" "$BUILD/bench.c" \
    >"$BUILD/bench.compile.log" 2>&1; then
  echo "FAIL  6d_bench (compile)"; cat "$BUILD/bench.compile.log"; exit 1
fi

# Extract types header
types="$BUILD/bench.types.h"
sed -n '/\/\*__PLANT_TYPES_BEGIN__\*\//,/\/\*__PLANT_TYPES_END__\*\//p' \
     "$BUILD/bench.c" | sed '1d;$d' > "$types"

if ! gcc -w -O0 -DPLANT_MALLOC_SIMULATE -include "$ROOT/tests/native/mock_ffi.h" \
      -include "$types" -I "$ROOT/runtime/c" "$BUILD/bench.c" \
      "$ROOT/runtime/c/plant_runtime.c" "$ROOT/runtime/c/plant_error.c" \
      "$ROOT/runtime/c/plant_report.c" "$ROOT/runtime/c/plant_report_json.c" \
      "$ROOT/runtime/c/plant_report_xml.c" "$ROOT/runtime/c/plant_report_html.c" \
      "$ROOT/runtime/c/plant_math.c" \
      "$ROOT/runtime/c/plant_tensor.c" \
      "$ROOT/runtime/c/plant_memory.c" \
      "$ROOT/tests/native/mock_ffi.c" \
      -lm -ldl -o "$BUILD/bench" \
      >>"$BUILD/bench.compile.log" 2>&1; then
  echo "FAIL  6d_bench (gcc)"; cat "$BUILD/bench.compile.log"; exit 1
fi

time "$BUILD/bench"
