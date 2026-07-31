#!/bin/sh
# PlantLang native integration test runner.
# Usage: sh tests/native/run_native_tests.sh [path-to-plantc]
set -u
PLANTC=${1:-bin/plantc}
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
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q 'usage: plantc'; then
  echo "PASS  cli --help"; pass=$((pass+1))
else
  echo "FAIL  cli --help"; fail=$((fail+1))
fi

out=$("$PLANTC" --version 2>&1)
rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q '1.0.0-phase5'; then
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
  if ! gcc -w -O0 -I "$ROOT/runtime/c" "$BUILD/$name.c" \
        "$ROOT/runtime/c/plant_runtime.c" -o "$BUILD/$name" \
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
echo "native tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
