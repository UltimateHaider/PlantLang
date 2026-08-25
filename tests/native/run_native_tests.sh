#!/bin/sh
# PlantLang native integration test runner.
# Usage: sh tests/native/run_native_tests.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_native_tests
rm -rf "$BUILD"
mkdir -p "$BUILD"
pass=0
fail=0

echo "== CLI checks =="

out=$("$PLANTC" --help 2>&1)
rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q 'usage: Chloroplast'; then
  echo "PASS  cli --help"; pass=$((pass+1))
else
  echo "FAIL  cli --help"; fail=$((fail+1))
fi

out=$("$PLANTC" --version 2>&1)
rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q '0.49.30'; then
  echo "PASS  cli --version"; pass=$((pass+1))
else
  echo "FAIL  cli --version"; fail=$((fail+1))
fi
"$PLANTC" "$BUILD/nope.plant" >/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "PASS  cli missing-file exits nonzero"; pass=$((pass+1))
else
  echo "FAIL  cli missing-file exits nonzero"; fail=$((fail+1))
fi

echo "== compile + run cases =="

for src in "$DIR"/*.plant; do
  name=$(basename "$src" .plant)
  [ -f "$DIR/$name.expected" ] || continue
  if ! "$PLANTC" "$src" "$BUILD/$name.c" >"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (compile)"; fail=$((fail+1)); continue
  fi
  # extract the generated FFI types block (v0.48.4) into a header so
  # mock_ffi.c sees the real struct typedefs + FFI-extension prototypes
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

echo "== type header (plant_types.h) =="

# tx_t must be defined once, in plant_types.h, and be visible to both
# plant_runtime.h (signatures use tx_t) and plant_compat.h (FFI
# statics) with no conflicts; linking against the real runtime also
# proves the runtime implementation resolves the same typedef.
if gcc -w -O0 -I "$ROOT/runtime/c" "$DIR/tx_types.c" \
      "$ROOT/runtime/c/plant_runtime.c" "$ROOT/tests/native/mock_ffi.c" \
      -lm -ldl -o "$BUILD/tx_types" \
      >"$BUILD/tx_types.compile.log" 2>&1 \
   && "$BUILD/tx_types" >/dev/null 2>&1; then
  echo "PASS  tx_types header decoupling"; pass=$((pass+1))
else
  echo "FAIL  tx_types header decoupling"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "native tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
