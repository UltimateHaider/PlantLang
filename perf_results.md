# Performance Results

- Generated: 2026-08-04 09:23 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.16 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 245 | 203520 | 22 | 80 |
| concat_bench | 2 | 178 | 204032 | 16 | 80 |
| concat_bench | 3 | 176 | 205568 | 16 | 80 |
| fast_loop | 1 | 203 | 1408 | 19 | 80 |
| fast_loop | 2 | 193 | 1536 | 18 | 80 |
| fast_loop | 3 | 268 | 1408 | 26 | 80 |
| numeric_bench | 1 | 8 | 1152 | 0 | 80 |
| numeric_bench | 2 | 10 | 1152 | 0 | 80 |
| numeric_bench | 3 | 8 | 1152 | 0 | 80 |
| perf_async | 1 | 8 | 0 | 0 | 80 |
| perf_async | 2 | 7 | 0 | 0 | 80 |
| perf_async | 3 | 12 | 0 | 0 | 80 |
| perf_concat | 1 | 180 | 201600 | 16 | 80 |
| perf_concat | 2 | 204 | 205568 | 19 | 80 |
| perf_concat | 3 | 214 | 203776 | 20 | 80 |
| perf_mixed | 1 | 308 | 302208 | 29 | 80 |
| perf_mixed | 2 | 326 | 295808 | 31 | 80 |
| perf_mixed | 3 | 305 | 297216 | 28 | 80 |
| safe_pool_bench | 1 | 173 | 1792 | 16 | 80 |
| safe_pool_bench | 2 | 229 | 1792 | 21 | 80 |
| safe_pool_bench | 3 | 253 | 1792 | 24 | 80 |
