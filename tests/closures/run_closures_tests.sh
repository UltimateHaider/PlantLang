#!/bin/sh
# PlantLang closures engine integration test runner.
# Optional $name.grep files hold fixed-string structural checks on
# the generated C: each non-empty line must appear, except lines
# prefixed with "!" which must NOT appear.
# Usage: sh tests/closures/run_closures_tests.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
# Cross-platform compiler flags: GCC 14+ promotes legacy C conversions
# to errors, and MSYS/Cygwin needs gnu11 for POSIX declarations.
CC=${CC:-gcc}
TEST_CFLAGS=${TEST_CFLAGS:--w -O0 -std=gnu11 -Wno-error=int-conversion -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_closures_tests
rm -rf "$BUILD"
mkdir -p "$BUILD"
pass=0
fail=0

# Fixtures may be checked out with CRLF (Windows); normalize to LF.
norm_expected() { tr -d '\r' < "$1" | awk 1; }

# v0.52.0 — precompile the test runtime once (5-10x faster tests).
if [ ! -f "$ROOT/build/libplant_test_runtime.a" ]; then
  make -s -C "$ROOT" test-runtime-lib || { echo "FAIL: runtime precompile"; exit 1; }
fi

for src in "$DIR"/*.plant; do
  name=$(basename "$src" .plant)
  [ -f "$DIR/$name.expected" ] || continue
  norm_expected "$DIR/$name.expected" > "$BUILD/$name.expected"
  if ! "$PLANTC" "$src" "$BUILD/$name.c" >"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (compile)"; fail=$((fail+1)); continue
  fi
  if ! "$CC" $TEST_CFLAGS -include "$ROOT/tests/native/mock_ffi.h" -I "$ROOT/runtime/c" "$BUILD/$name.c" \
        "$ROOT/tests/native/mock_ffi.c" \
        "$ROOT/build/libplant_test_runtime.a" \
        -lm -ldl -o "$BUILD/$name" \
        >>"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (gcc)"; fail=$((fail+1)); continue
  fi
  # optional structural checks on the generated C
  if [ -f "$DIR/$name.grep" ]; then
    tr -d '\r' < "$DIR/$name.grep" > "$BUILD/$name.grep"
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
    done < "$BUILD/$name.grep"
    if [ "$gok" -eq 0 ]; then
      fail=$((fail+1)); continue
    fi
  fi
  if "$BUILD/$name" 2>&1 | tr -d '\r' | diff - "$BUILD/$name.expected" >/dev/null; then
    echo "PASS  $name"; pass=$((pass+1))
  else
    echo "FAIL  $name (output)"; fail=$((fail+1))
  fi
done

echo "----------------------------------------"
echo "closures tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
