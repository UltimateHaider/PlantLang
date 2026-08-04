# Performance Results

- Generated: 2026-08-04 10:10 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.16 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 134 | 198528 | 12 | 96 |
| concat_bench | 2 | 131 | 198144 | 11 | 96 |
| concat_bench | 3 | 123 | 194816 | 11 | 96 |
| fast_loop | 1 | 75 | 1536 | 6 | 96 |
| fast_loop | 2 | 73 | 1408 | 6 | 96 |
| fast_loop | 3 | 71 | 1664 | 6 | 96 |
| numeric_bench | 1 | 6 | 1280 | 0 | 96 |
| numeric_bench | 2 | 10 | 1280 | 0 | 96 |
| numeric_bench | 3 | 6 | 1280 | 0 | 96 |
| perf_async | 1 | 8 | 0 | 0 | 96 |
| perf_async | 2 | 8 | 0 | 0 | 96 |
| perf_async | 3 | 6 | 0 | 0 | 96 |
| perf_concat | 1 | 138 | 204032 | 12 | 96 |
| perf_concat | 2 | 179 | 195328 | 16 | 96 |
| perf_concat | 3 | 135 | 197376 | 12 | 96 |
| perf_mixed | 1 | 187 | 303360 | 16 | 96 |
| perf_mixed | 2 | 162 | 299264 | 14 | 96 |
| perf_mixed | 3 | 186 | 299392 | 18 | 96 |
| safe_pool_bench | 1 | 99 | 1792 | 9 | 96 |
| safe_pool_bench | 2 | 87 | 1920 | 8 | 96 |
| safe_pool_bench | 3 | 92 | 1792 | 8 | 96 |
| smart_bench | 1 | 1104 | 1664 | 109 | 96 |
| smart_bench | 2 | 1075 | 1664 | 106 | 96 |
| smart_bench | 3 | 1058 | 1792 | 104 | 96 |
