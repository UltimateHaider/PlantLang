#!/bin/sh
# tests/native/leak_gate.sh — v0.63.4 TD-047 exact leak gate.
#
# Recompiles the runtime with a tracking allocator into $TMPDIR (never into
# the repo) and asserts that each MAP-overwrite test ends at the same live
# byte count as a baseline program. A leak is a positive net delta:
#   tensor overwrite  -> +104 B before the fix, 0 B after
#   numeric overwrite -> +11 B  before the fix, 0 B after
#
# Usage: sh tests/native/leak_gate.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
CC=${CC:-gcc}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_leak_gate
rm -rf "$BUILD"
mkdir -p "$BUILD/rt" "$BUILD/t"

if [ ! -f "$ROOT/tests/native/alloc_track.c" ]; then
    echo "leak-gate: FAIL (tests/native/alloc_track.c missing)"
    exit 1
fi

CFLAGS="-w -O0 -std=gnu11 -DPLANT_MALLOC_SIMULATE \
 -Wno-error=int-conversion -Wno-error=implicit-function-declaration \
 -Wno-error=incompatible-pointer-types \
 -I $ROOT/runtime/c -I $ROOT/tests/native"
TRACK="-include $ROOT/tests/native/alloc_track.h \
 -include $ROOT/tests/native/mock_ffi.h \
 -Dmalloc=track_malloc -Dfree=track_free \
 -Drealloc=track_realloc -Dcalloc=track_calloc"

SRCS="plant_runtime plant_error plant_report plant_report_json \
 plant_report_xml plant_report_html plant_math plant_tensor plant_memory \
 plant_complex plant_geo plant_autograd plant_ad2"

for s in $SRCS; do
    if ! $CC $CFLAGS $TRACK -c "$ROOT/runtime/c/$s.c" -o "$BUILD/rt/$s.o" \
            2> "$BUILD/rt/$s.err"; then
        echo "leak-gate: FAIL (cannot compile runtime unit $s)"
        head -5 "$BUILD/rt/$s.err"
        exit 1
    fi
done

if ! $CC $CFLAGS -c "$ROOT/tests/native/alloc_track.c" \
        -o "$BUILD/rt/alloc_track.o" 2> "$BUILD/rt/alloc_track.err"; then
    echo "leak-gate: FAIL (cannot compile alloc_track.c)"
    head -5 "$BUILD/rt/alloc_track.err"
    exit 1
fi

# run_case NAME SOURCE -> prints live_bytes, or nothing on failure
run_case() {
    _name=$1
    _src=$2
    if ! "$PLANTC" "$_src" "$BUILD/t/$_name.c" > "$BUILD/t/$_name.log" 2>&1; then
        echo "  compile: $(tail -1 "$BUILD/t/$_name.log")" >&2
        return 1
    fi
    if ! $CC $CFLAGS $TRACK -c "$BUILD/t/$_name.c" \
            -o "$BUILD/t/$_name.o" 2> "$BUILD/t/$_name.cc.log"; then
        echo "  gcc: $(head -3 "$BUILD/t/$_name.cc.log" | tr '\n' ' ')" >&2
        return 1
    fi
    if ! $CC -w -O0 "$BUILD/t/$_name.o" "$BUILD"/rt/*.o \
            -lm -o "$BUILD/t/$_name.exe" 2> "$BUILD/t/$_name.ld.log"; then
        echo "  link: $(head -3 "$BUILD/t/$_name.ld.log" | tr '\n' ' ')" >&2
        return 1
    fi
    if ! "$BUILD/t/$_name.exe" > "$BUILD/t/$_name.out" 2>&1; then
        echo "  run: exit $?" >&2
        return 1
    fi
    sed -n 's/.*live_bytes=\([0-9][0-9]*\).*/\1/p' "$BUILD/t/$_name.out" | head -1
}

printf 'SHOW "leak-gate-baseline".\n' > "$BUILD/t/baseline.plant"
BASE=$(run_case baseline "$BUILD/t/baseline.plant")
if [ -z "$BASE" ]; then
    echo "leak-gate: FAIL (baseline did not report TRACK live_bytes)"
    exit 1
fi

fail=0
for t in td047_map_overwrite td047_map_num_overwrite; do
    src="$ROOT/tests/native/$t.plant"
    bytes=$(run_case "$t" "$src")
    if [ -z "$bytes" ]; then
        echo "FAIL  $t (build/run)"
        fail=1
        continue
    fi
    net=$((bytes - BASE))
    if [ "$net" -gt 0 ]; then
        echo "FAIL  $t (live ${bytes} B, baseline ${BASE} B, net +${net} B)"
        fail=1
    else
        echo "PASS  $t (net ${net} B)"
    fi
done

if [ "$fail" -eq 0 ]; then
    echo "leak-gate: PASS (baseline ${BASE} B)"
    exit 0
fi
echo "leak-gate: FAIL"
exit 1
