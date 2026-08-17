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
# stale scratch files from the fs_append regression test (appends must
# start from an empty target each run)
rm -f /tmp/plantlang_fs_append_*.txt
# v0.48.32: local mock HTTP server for the HARVEST tests (started
# only when such tests exist; python3 required, else tests fail).
# v0.48.33: also started when LISTEN tests exist (listen_busy binds
# the mock server's port to provoke a bind failure).
MOCK_PID=
if { ls "$DIR"/harvest_*.plant >/dev/null 2>&1 || ls "$DIR"/listen_*.plant >/dev/null 2>&1; } && command -v python3 >/dev/null 2>&1; then
  python3 "$DIR/mock_http_server.py" >"$BUILD/mock_http_server.log" 2>&1 &
  MOCK_PID=$!
  sleep 1
  # v0.49.1: reap the server on every exit path (normal finish, SIGINT,
  # SIGTERM) so no leaked process holds port 41234 for the next run.
  trap 'if [ -n "$MOCK_PID" ]; then kill "$MOCK_PID" 2>/dev/null; wait "$MOCK_PID" 2>/dev/null; fi' EXIT INT TERM
fi
pass=0
fail=0

for src in "$DIR"/*.plant; do
  name=$(basename "$src" .plant)
  [ -f "$DIR/$name.expected" ] || continue
  if [ -f "$DIR/$name.invalid" ]; then
    # negative test: the source must FAIL to compile, and the compiler
    # log must contain every diagnostic line from the .expected file
    if ! "$PLANTC" "$src" "$BUILD/$name.c" >"$BUILD/$name.compile.log" 2>&1; then
      ok=1
      while IFS= read -r want; do
        grep -Fq -- "$want" "$BUILD/$name.compile.log" || ok=0
      done < "$DIR/$name.expected"
      if [ "$ok" -eq 1 ]; then
        echo "PASS  $name"; pass=$((pass+1))
      else
        echo "FAIL  $name (diagnostic mismatch)"; fail=$((fail+1))
      fi
    else
      echo "FAIL  $name (compiled, should have failed)"; fail=$((fail+1))
    fi
    continue
  fi
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
  # v0.48.33: LISTEN tests are one-shot servers — the .plant binary
  # blocks on accept until a python client drives it. The client's
  # reply report is appended after the server's stdout so one
  # .expected file covers both sides. listen_busy needs no client
  # (the mock server already holds its port).
  if [ "${name#listen_}" != "$name" ] && [ "$name" != "listen_busy" ]; then
    "$BUILD/$name" >"$BUILD/$name.out" 2>&1 &
    spid=$!
    case "$name" in
      listen_malformed)
        python3 "$DIR/listen_client.py" malformed 41237 >"$BUILD/$name.client" 2>&1
        ;;
      *)
        python3 "$DIR/listen_client.py" request 41235 /hello?q=1 ping abc123 >"$BUILD/$name.client" 2>&1
        ;;
    esac
    wait "$spid" 2>/dev/null
    # v0.49.1: normalize the fixture to always end with a newline so a
    # missing trailing \n in an .expected file cannot fail the diff.
    awk 1 "$DIR/$name.expected" > "$BUILD/$name.expected"
    if cat "$BUILD/$name.out" "$BUILD/$name.client" 2>/dev/null | diff - "$BUILD/$name.expected" >/dev/null; then
      echo "PASS  $name"; pass=$((pass+1))
    else
      echo "FAIL  $name (output)"; fail=$((fail+1))
    fi
    continue
  fi
  awk 1 "$DIR/$name.expected" > "$BUILD/$name.expected"
  if "$BUILD/$name" 2>&1 | diff - "$BUILD/$name.expected" >/dev/null; then
    echo "PASS  $name"; pass=$((pass+1))
  else
    echo "FAIL  $name (output)"; fail=$((fail+1))
  fi
done

echo "----------------------------------------"
echo "regression tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]