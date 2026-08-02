#!/bin/sh
# PlantLang benchmark runner — compiles tests/perf/*.plant, runs each
# benchmark 3x, and writes perf_results.md with exec time, peak RSS,
# CPU usage and toolchain versions.
# Usage: sh tests/perf/run_perf.sh [path-to-Chloroplast]
set -u
PLANTC=${1:-bin/Chloroplast}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${TMPDIR:-/tmp}/plantlang_perf
rm -rf "$BUILD"
mkdir -p "$BUILD"
OUT="$ROOT/perf_results.md"

nsec() { date +%s%N; }

# Measure a command: real ns + peak RSS KB (polled from /proc) + cpu ticks
measure() {
  bin=$1
  start=$(nsec)
  "$bin" >/dev/null 2>&1 &
  pid=$!
  peak=0
  cpu=0
  while kill -0 "$pid" 2>/dev/null; do
    rss=$(awk '/VmHWM/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0)
    [ -n "$rss" ] && [ "$rss" -gt "$peak" ] && peak=$rss
    ticks=$(awk '{print $14+$15}' "/proc/$pid/stat" 2>/dev/null || echo 0)
    [ -n "$ticks" ] && [ "$ticks" -gt "$cpu" ] && cpu=$ticks
    sleep 0.002
  done
  wait "$pid"
  rc=$?
  end=$(nsec)
  real_ms=$(( (end - start) / 1000000 ))
  echo "$real_ms $peak $cpu $rc"
}

version=$("$PLANTC" --version 2>&1 | tr -d '\n')
gccver=$(gcc --version 2>/dev/null | head -1)
platform=$(uname -srm)

{
  echo "# Performance Results"
  echo
  echo "- Generated: $(date -u '+%Y-%m-%d %H:%M UTC')"
  echo "- Toolchain: $platform"
  echo "- Compiler:  $version"
  echo "- Host CC:   $gccver"
  echo "- Method: 3 runs per benchmark; real time via \`date +%s%N\`, peak RSS polled from /proc/PID/status (VmHWM)."
  echo
  echo "| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |"
  echo "|---|---|---|---|---|---|"
} > "$OUT"

for src in "$DIR"/*.plant; do
  name=$(basename "$src" .plant)
  if ! "$PLANTC" "$src" "$BUILD/$name.c" >"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (compile) — see $BUILD/$name.compile.log"; continue
  fi
  if ! gcc -w -O2 -I "$ROOT/runtime/c" "$BUILD/$name.c" \
        "$ROOT/runtime/c/plant_runtime.c" -lm -ldl -o "$BUILD/$name" \
        >>"$BUILD/$name.compile.log" 2>&1; then
    echo "FAIL  $name (gcc) — see $BUILD/$name.compile.log"; continue
  fi
  echo "RUN   $name"
  for run in 1 2 3; do
    set -- $(measure "$BUILD/$name")
    real_ms=$1; peak=$2; cpu=$3; rc=$4
    echo "| $name | $run | $real_ms | $peak | $cpu | $rc |" >> "$OUT"
  done
done

echo "== perf_results.md written =="
