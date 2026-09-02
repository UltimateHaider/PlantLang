# v0.49.57: Smoke test runner — rapid validation of core language features.
# Usage: sh tests/smoke/run_smoke_tests.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_smoke_tests
rm -rf "$BUILD"
mkdir -p "$BUILD"

pass=0
fail=0

echo "== Smoke tests =="

for src in "$DIR"/*.plant; do
  name=$(basename "$src" .plant)
  [ -f "$DIR/$name.expected" ] || continue

  if ! "$PLANTC" "$src" "$BUILD/$name.c" >"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (compile)"; fail=$((fail+1)); continue
  fi

  if ! gcc -w -O0 -I "$ROOT/runtime/c" "$BUILD/$name.c" \
        "$ROOT/runtime/c/plant_runtime.c" "$ROOT/runtime/c/plant_error.c" \
        "$ROOT/runtime/c/plant_report.c" "$ROOT/runtime/c/plant_report_json.c" \
        "$ROOT/runtime/c/plant_report_xml.c" "$ROOT/runtime/c/plant_report_html.c" \
        "$ROOT/tests/native/mock_ffi.c" \
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
echo "smoke tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
