#!/bin/sh
# PlantLang closures engine integration test runner.
# Optional $name.grep files hold fixed-string structural checks on
# the generated C: each non-empty line must appear, except lines
# prefixed with "!" which must NOT appear.
# Usage: sh tests/closures/run_closures_tests.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_closures_tests
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
  if ! gcc -w -O0 -include "$ROOT/tests/native/mock_ffi.h" -I "$ROOT/runtime/c" "$BUILD/$name.c" \
        "$ROOT/runtime/c/plant_runtime.c" "$ROOT/tests/native/mock_ffi.c" \
        -lm -ldl -o "$BUILD/$name" \
        >>"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (gcc)"; fail=$((fail+1)); continue
  fi
  # optional structural checks on the generated C
  if [ -f "$DIR/$name.grep" ]; then
    gok=1
    while IFS= read -r pat; do
      [ -z "$pat" ] && continue
      if [ "${pat#!}" != "$pat" ]; then
        if grep -qF -- "${pat#!}" "$BUILD/$name.c"; then
          echo "FAIL  $name (grep forbidden: ${pat#!})"; gok=0; break
        fi
      elif ! grep -qF -- "$pat" "$BUILD/$name.c"; then
        echo "FAIL  $name (grep missing: $pat)"; gok=0; break
      fi
    done < "$DIR/$name.grep"
    if [ "$gok" -eq 0 ]; then
      fail=$((fail+1)); continue
    fi
  fi
  if "$BUILD/$name" 2>&1 | diff - "$DIR/$name.expected" >/dev/null; then
    echo "PASS  $name"; pass=$((pass+1))
  else
    echo "FAIL  $name (output)"; fail=$((fail+1))
  fi
done

echo "----------------------------------------"
echo "closures tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
