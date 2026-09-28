#!/bin/sh
# tests/perf/run_6d_bench.sh — v0.51.2d.2
# Compiles the 6D create+free benchmark and times the RUNTIME.
# Usage: sh tests/perf/run_6d_bench.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_6d_bench
OUTPUT_FILE="$DIR/6d_bench_results.txt"
rm -rf "$BUILD"
mkdir -p "$BUILD"

if ! "$PLANTC" "$DIR/tensor_6d_bench.plant" "$BUILD/bench.c" \
    >"$BUILD/compile.log" 2>&1; then
  echo "FAIL  6d_bench (compile)"; cat "$BUILD/compile.log"; exit 1
fi

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
      >>"$BUILD/compile.log" 2>&1; then
  echo "FAIL  6d_bench (gcc)"; cat "$BUILD/compile.log"; exit 1
fi

{
  echo "=== 6D Tensor Benchmark ==="
  echo "Date: $(date)"
  echo "Host: $(hostname)"
  echo "OS: $(uname -s -r)"
  echo "Compiler: $(cc --version | head -1)"
  echo ""
  echo "--- 1000 iterations, 6D tensor create+free (runtime) ---"
} > "$OUTPUT_FILE"

# POSIX sh has no `time` reserved word here; measure wall clock via date.
start_ns=$(date +%s%N)
"$BUILD/bench" >> "$OUTPUT_FILE" 2>&1
end_ns=$(date +%s%N)
elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
echo "Elapsed: ${elapsed_ms} ms" >> "$OUTPUT_FILE"

cat "$OUTPUT_FILE"
echo "Results saved to: $OUTPUT_FILE"
