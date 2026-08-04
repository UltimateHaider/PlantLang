# Performance Results

- Generated: 2026-08-04 08:16 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.15 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 190 | 204800 | 17 | 80 |
| concat_bench | 2 | 130 | 205824 | 11 | 80 |
| concat_bench | 3 | 137 | 193536 | 12 | 80 |
| fast_loop | 1 | 82 | 1408 | 7 | 80 |
| fast_loop | 2 | 73 | 1408 | 6 | 80 |
| fast_loop | 3 | 67 | 1408 | 6 | 80 |
| numeric_bench | 1 | 10 | 1152 | 0 | 80 |
| numeric_bench | 2 | 7 | 1280 | 0 | 80 |
| numeric_bench | 3 | 6 | 1152 | 0 | 80 |
| perf_async | 1 | 5 | 0 | 0 | 80 |
| perf_async | 2 | 9 | 0 | 0 | 80 |
| perf_async | 3 | 6 | 0 | 0 | 80 |
| perf_concat | 1 | 124 | 201088 | 11 | 80 |
| perf_concat | 2 | 134 | 205312 | 12 | 80 |
| perf_concat | 3 | 137 | 203776 | 11 | 80 |
| perf_mixed | 1 | 280 | 304640 | 27 | 80 |
| perf_mixed | 2 | 205 | 296832 | 20 | 80 |
| perf_mixed | 3 | 169 | 301568 | 14 | 80 |
