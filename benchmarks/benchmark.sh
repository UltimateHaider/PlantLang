#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
# Chloroplast Benchmark Suite (v0.47.4)
#   plant (Chloroplast → C → gcc -O2) vs native C (gcc -O2) vs Python 3
# Usage: bash benchmarks/benchmark.sh
# ═══════════════════════════════════════════════════════════════════
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PLANTC="$ROOT/bin/Chloroplast"
OUT="$ROOT/benchmarks/.out"
mkdir -p "$OUT"

now_ms() { date +%s%3N; }

# t_ms <cmd...> — run once, print elapsed ms
t_ms() {
  local t0 t1
  t0=$(now_ms)
  "$@" >/dev/null 2>&1
  t1=$(now_ms)
  echo $((t1 - t0))
}

# best_ms <cmd...> — best (min) of 3 runs
best_ms() {
  local a b c
  a=$(t_ms "$@"); b=$(t_ms "$@"); c=$(t_ms "$@")
  [ "$b" -lt "$a" ] && a=$b
  [ "$c" -lt "$a" ] && a=$c
  echo "$a"
}

# max_rss_kb <cmd...> — peak VmRSS by polling /proc (no GNU time here)
# subshell-free polling: forking awk per sample is too slow for ~2 ms runs
max_rss_kb() {
  local pid rss max=0 ln
  "$@" >/dev/null 2>&1 &
  pid=$!
  while kill -0 "$pid" 2>/dev/null; do
    rss=
    { while IFS= read -r ln; do
        case "$ln" in VmRSS:*) set -- $ln; rss=$2; break;; esac
      done; } 2>/dev/null < "/proc/$pid/status"
    [ -n "$rss" ] && [ "$rss" -gt "$max" ] && max=$rss
  done
  wait "$pid"
  echo "$max"
}

# compile_plant <name> [shim]
compile_plant() {
  local name=$1 shim="${2:-}"
  if ! "$PLANTC" "benchmarks/$name.plant" "$OUT/$name.c" >"$OUT/$name.compile.log" 2>&1; then
    echo "FAIL: compile benchmarks/$name.plant (see $OUT/$name.compile.log)"
    return 1
  fi
  local inc="" link=""
  if [ -n "$shim" ]; then
    inc="-include benchmarks/$shim.h"
    link="benchmarks/$shim.c"
  fi
  if ! gcc -w -O2 $inc -I runtime/c "$OUT/$name.c" runtime/c/plant_runtime.c \
        tests/native/mock_ffi.c $link -lm -ldl -o "$OUT/$name" \
        >>"$OUT/$name.compile.log" 2>&1; then
    echo "FAIL: gcc -O2 for benchmarks/$name (see $OUT/$name.compile.log)"
    return 1
  fi
}

echo "════════════════════════════════════════════════════════════"
echo " Chloroplast Benchmark Suite — $("$PLANTC" --version 2>/dev/null)"
echo " $(uname -s) $(uname -m) | gcc $(gcc --version 2>/dev/null | head -1)"
echo " $(python3 --version 2>&1) | $(uname -r)"
echo "════════════════════════════════════════════════════════════"

# ────────────────────────────────────────────────────────────────
echo
echo "[1] Execution speed (wall ms; plant 1 run, C/Python best of 3)"
echo "    fib(35) recursive | loop 10M iter | json 5000 parse+stringify"
echo "    factors: vs C = plant time ÷ C time | vs Py = Python time ÷ plant time"
echo
printf "  %-9s %12s %12s %12s %10s %10s\n" bench Chloroplast C_gccO2 Python3 "vs C" "vs Py"
declare -A RATIO_C RATIO_PY
for b in fib json loop; do
  compile_plant "$b" || continue
  p=$(t_ms "$OUT/$b")
  gcc -w -O2 -I runtime/c "benchmarks/c/$b.c" runtime/c/plant_runtime.c -lm -ldl -o "$OUT/c_$b"
  c=$(best_ms "$OUT/c_$b")
  pyf="$b.py"; [ "$b" = json ] && pyf="json_bench.py"
  py=$(best_ms python3 "benchmarks/py/$pyf")
  rc=$(awk "BEGIN{printf \"%.2f\", $p/$c}")
  rp=$(awk "BEGIN{printf \"%.1f\", $py/$p}")
  RATIO_C[$b]=$rc; RATIO_PY[$b]=$rp
  printf "  %-9s %12s %12s %12s %8sx %8sx\n" "$b" "$p" "$c" "$py" "$rc" "$rp"
done

# ────────────────────────────────────────────────────────────────
echo
echo "[2] FFI overhead (plant, 10M iters; value-return 200K iters)"
if compile_plant ffi_overhead ffi_shim; then
  fout=$("$OUT/ffi_overhead")
  echo "$fout" | sed 's/^/    /'
  pms=$(echo "$fout" | sed -n 's/^plant-ms=//p')
  nms=$(echo "$fout" | sed -n 's/^noop-ms=//p')
  vms=$(echo "$fout" | sed -n 's/^value-ms=//p')
  rms=$(echo "$fout" | sed -n 's/^ref-ms=//p')
  if [ -n "$pms" ] && [ -n "$nms" ] && [ -n "$rms" ]; then
    novh=$(( (nms - pms) / 10 ))     # ns/call over baseline, 10M calls
    rovh=$(( (rms - pms) / 10 ))
    [ "$novh" -lt 0 ] && novh=0
    [ "$rovh" -lt 0 ] && rovh=0
    echo "    → external call (void) overhead:      ${novh} ns/call"
    echo "    → REF pass-by-reference overhead:    ${rovh} ns/call"
  fi
  if [ -n "$vms" ]; then
    vovh=$(( (vms - pms / 50) * 5 ))  # 200K calls → ns/call = d_ms × 5
    [ "$vovh" -lt 0 ] && vovh=0
    echo "    → TX value-return cost (ffi_add):    ${vovh} ns/call"
  fi
fi

# ────────────────────────────────────────────────────────────────
echo
echo "[3] Build performance (clean tree)"
chmod +x dist/Chloroplast 2>/dev/null
t0=$(now_ms); make clean >/dev/null 2>&1; t1=$(now_ms)
echo "    make clean   $((t1 - t0)) ms"
t0=$(now_ms); make all >/dev/null 2>&1; t1=$(now_ms)
echo "    make all     $((t1 - t0)) ms   (v1→v2→v3 full bootstrap)"
t0=$(now_ms); conv=$(make self 2>&1); t1=$(now_ms)
echo "    make self    $((t1 - t0)) ms   → $(echo "$conv" | grep -o 'SELF-HOSTING CONVERGED ([0-9]* bytes)')"

# ────────────────────────────────────────────────────────────────
echo
echo "[4] Memory footprint (max RSS, kB)"
printf "  %-9s %12s %12s %12s\n" bench Chloroplast C_gccO2 Python3
for b in fib json loop; do
  [ -x "$OUT/$b" ] || continue
  rp=$(max_rss_kb "$OUT/$b")
  rc=$(max_rss_kb "$OUT/c_$b")
  pyf="$b.py"; [ "$b" = json ] && pyf="json_bench.py"
  rpy=$(max_rss_kb python3 "benchmarks/py/$pyf")
  printf "  %-9s %12s %12s %12s\n" "$b" "$rp" "$rc" "$rpy"
done

# ────────────────────────────────────────────────────────────────
echo
echo "[5] Summary — speedup factors (Chloroplast = 1.0x baseline)"
printf "  %-9s %12s %12s\n" bench "C (gcc -O2)" Python3
for b in fib json loop; do
  printf "  %-9s %10sx %10sx\n" "$b" "${RATIO_C[$b]:-?}" "${RATIO_PY[$b]:-?}"
done
echo
echo "Benchmark artifacts: $OUT"
echo "════════════════════════════════════════════════════════════"
