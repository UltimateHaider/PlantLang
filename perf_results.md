# Performance Results

- Generated: 2026-08-04 11:12 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.17 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 219 | 203520 | 19 | 96 |
| concat_bench | 2 | 253 | 206464 | 23 | 96 |
| concat_bench | 3 | 178 | 196608 | 16 | 96 |
| fast_loop | 1 | 105 | 1664 | 10 | 96 |
| fast_loop | 2 | 96 | 1536 | 7 | 96 |
| fast_loop | 3 | 96 | 1408 | 7 | 96 |
| numeric_bench | 1 | 10 | 1152 | 0 | 96 |
| numeric_bench | 2 | 7 | 1152 | 0 | 96 |
| numeric_bench | 3 | 8 | 1280 | 0 | 96 |
| perf_async | 1 | 9 | 0 | 0 | 96 |
| perf_async | 2 | 8 | 0 | 0 | 96 |
| perf_async | 3 | 8 | 0 | 0 | 96 |
| perf_concat | 1 | 175 | 199936 | 16 | 96 |
| perf_concat | 2 | 182 | 204800 | 16 | 96 |
| perf_concat | 3 | 253 | 204288 | 23 | 96 |
| perf_mixed | 1 | 341 | 302208 | 33 | 96 |
| perf_mixed | 2 | 226 | 304128 | 21 | 96 |
| perf_mixed | 3 | 235 | 303872 | 21 | 96 |
| persistent_bench | 1 | 405 | 1664 | 40 | 96 |
| persistent_bench | 2 | 429 | 1792 | 40 | 96 |
| persistent_bench | 3 | 387 | 1664 | 37 | 96 |
| safe_pool_bench | 1 | 131 | 1792 | 11 | 96 |
| safe_pool_bench | 2 | 125 | 1792 | 11 | 96 |
| safe_pool_bench | 3 | 133 | 1792 | 11 | 96 |
| smart_bench | 1 | 1431 | 1792 | 142 | 96 |
| smart_bench | 2 | 1470 | 1792 | 145 | 96 |
| smart_bench | 3 | 1573 | 1792 | 156 | 96 |
