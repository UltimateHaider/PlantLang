# Performance Results

- Generated: 2026-08-03 05:31 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.4 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 150 | 196864 | 13 | 80 |
| concat_bench | 2 | 123 | 204800 | 11 | 80 |
| concat_bench | 3 | 120 | 189568 | 11 | 80 |
| numeric_bench | 1 | 5 | 1152 | 0 | 80 |
| numeric_bench | 2 | 6 | 1152 | 0 | 80 |
| numeric_bench | 3 | 5 | 1280 | 0 | 80 |
| perf_async | 1 | 9 | 0 | 0 | 80 |
| perf_async | 2 | 7 | 0 | 0 | 80 |
| perf_async | 3 | 7 | 0 | 0 | 80 |
| perf_concat | 1 | 148 | 196736 | 12 | 80 |
| perf_concat | 2 | 120 | 203648 | 10 | 80 |
| perf_concat | 3 | 128 | 201984 | 11 | 80 |
| perf_mixed | 1 | 175 | 292736 | 16 | 80 |
| perf_mixed | 2 | 157 | 295808 | 14 | 80 |
| perf_mixed | 3 | 166 | 303744 | 15 | 80 |
