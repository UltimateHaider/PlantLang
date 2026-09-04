#!/bin/sh
# PlantLang — Performance Benchmark (v0.49.60c)
# Tracks compilation times for `make all`, `make self`, and `make test`.
# Writes results to benchmarks/bench_results.md (creates directory if missing).
# Usage: sh scripts/benchmark.sh

set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

TS=$(date +"%Y%m%d_%H%M%S")
OUTDIR="$ROOT/benchmarks"
mkdir -p "$OUTDIR"
RESULTS="$OUTDIR/bench_${TS}.md"

run_bench() {
    target="$1"
    label="$2"
    start=$(date +%s.%N)
    if make -s "$target" >/dev/null 2>&1; then
        end=$(date +%s.%N)
        elapsed=$(echo "$end - $start" | bc 2>/dev/null || echo "N/A")
        printf "  %-20s %s sec\n" "$label" "$elapsed"
        printf "| %s | %s |\n" "$label" "$elapsed" >> "$RESULTS"
    else
        printf "  %-20s FAILED\n" "$label"
        printf "| %s | FAILED |\n" "$label" >> "$RESULTS"
    fi
}

echo "== PlantLang Benchmark v0.49.57 ($TS) =="
echo ""
echo "Target              Time"
echo "───────────────────────────"

cat > "$RESULTS" <<EOF
# PlantLang Benchmark — $(date -u +"%Y-%m-%dT%H:%M:%SZ")

| Target | Time (seconds) |
|--------|----------------|
EOF

make -s clean >/dev/null 2>&1
run_bench all  "make all"
run_bench self "make self"
run_bench test "make test"

echo ""
echo "Results written to: $RESULTS"

# Append to the rolling summary if it exists
SUMMARY="$OUTDIR/bench_results.md"
echo "| $TS | $(grep -c 'PASS' "$RESULTS" 2>/dev/null || echo 0) pass | $(grep -c 'FAILED' "$RESULTS" 2>/dev/null || echo 0) fail |" >> "$SUMMARY" 2>/dev/null || true
