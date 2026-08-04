# Performance Results

- Generated: 2026-08-04 09:25 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.16 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 219 | 199808 | 21 | 80 |
| concat_bench | 2 | 127 | 203648 | 11 | 80 |
| concat_bench | 3 | 129 | 205952 | 11 | 80 |
| fast_loop | 1 | 100 | 1408 | 9 | 80 |
| fast_loop | 2 | 93 | 1408 | 8 | 80 |
| fast_loop | 3 | 90 | 1408 | 8 | 80 |
| numeric_bench | 1 | 5 | 1280 | 0 | 80 |
| numeric_bench | 2 | 15 | 1280 | 0 | 80 |
| numeric_bench | 3 | 23 | 0 | 0 | 80 |
| perf_async | 1 | 5 | 0 | 0 | 80 |
| perf_async | 2 | 5 | 0 | 0 | 80 |
| perf_async | 3 | 5 | 0 | 0 | 80 |
| perf_concat | 1 | 104 | 206336 | 9 | 80 |
| perf_concat | 2 | 110 | 206080 | 9 | 80 |
| perf_concat | 3 | 154 | 203136 | 14 | 80 |
| perf_mixed | 1 | 223 | 296704 | 19 | 80 |
| perf_mixed | 2 | 154 | 297088 | 14 | 80 |
| perf_mixed | 3 | 227 | 295936 | 21 | 80 |
| safe_pool_bench | 1 | 112 | 1664 | 10 | 80 |
| safe_pool_bench | 2 | 92 | 1664 | 8 | 80 |
| safe_pool_bench | 3 | 86 | 1792 | 8 | 80 |
