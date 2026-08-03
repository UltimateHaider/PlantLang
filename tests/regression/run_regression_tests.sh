#!/bin/sh
# PlantLang numeric/FFI regression test runner (v0.48.5).
# Compiles each tests/regression/*.plant to C, builds with the mock FFI
# library + runtime, and diffs stdout against the .expected file.
# Usage: sh tests/regression/run_regression_tests.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_regression_tests
rm -rf "$BUILD"
mkdir -p "$BUILD"
pass=0
fail=0

for src in "$DIR"/*.plant; do
  name=$(basename "$src" .plant)
  [ -f "$DIR/$name.expected" ] || continue
  if ! "$PLANTC" "$src" "$BUILD/$name.c" >"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (compile)"; fail=$((fail+1)); continue
  fi
  # extract the generated FFI types block into a header so mock_ffi.c
  # sees the real struct typedefs + FFI-extension prototypes
  types="$BUILD/$name.types.h"
  sed -n '/\/\*__PLANT_TYPES_BEGIN__\*\//,/\/\*__PLANT_TYPES_END__\*\//p' \
       "$BUILD/$name.c" | sed '1d;$d' > "$types"
  if ! gcc -w -O0 -include "$ROOT/tests/native/mock_ffi.h" \
        -include "$types" -I "$ROOT/runtime/c" "$BUILD/$name.c" \
        "$ROOT/runtime/c/plant_runtime.c" "$ROOT/tests/native/mock_ffi.c" \
        -lm -ldl -o "$BUILD/$name" \
        >>"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (gcc)"; fail=$((fail+1)); continue
  fi
  if "$BUILD/$name" 2>&1 | diff - "$DIR/$name.expected" >/dev/null; then
    echo "PASS  $name"; pass=$((pass+1))
  else
    echo "FAIL  $name (output)"; fail=$((fail+1))
  fi
done

echo "----------------------------------------"
echo "regression tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]