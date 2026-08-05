# Performance Results

- Generated: 2026-08-05 07:29 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.19 (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| concat_bench | 1 | 123 | 197504 | 10 | 96 |
| concat_bench | 2 | 121 | 196352 | 11 | 96 |
| concat_bench | 3 | 117 | 199680 | 10 | 96 |
| fast_loop | 1 | 86 | 1408 | 7 | 96 |
| fast_loop | 2 | 91 | 1536 | 8 | 96 |
| fast_loop | 3 | 83 | 1408 | 7 | 96 |
| numeric_bench | 1 | 6 | 1280 | 0 | 96 |
| numeric_bench | 2 | 7 | 1280 | 0 | 96 |
| numeric_bench | 3 | 7 | 1024 | 0 | 96 |
| perf_async | 1 | 6 | 0 | 0 | 96 |
| perf_async | 2 | 6 | 0 | 0 | 96 |
| perf_async | 3 | 6 | 0 | 0 | 96 |
| perf_concat | 1 | 117 | 200448 | 10 | 96 |
| perf_concat | 2 | 113 | 199680 | 10 | 96 |
| perf_concat | 3 | 120 | 202112 | 10 | 96 |
| perf_mixed | 1 | 166 | 304128 | 15 | 96 |
| perf_mixed | 2 | 160 | 299392 | 14 | 96 |
| perf_mixed | 3 | 164 | 300672 | 15 | 96 |
| persistent_bench | 1 | 300 | 1664 | 29 | 96 |
| persistent_bench | 2 | 331 | 1792 | 32 | 96 |
| persistent_bench | 3 | 364 | 1792 | 35 | 96 |
| safe_pool_bench | 1 | 115 | 1920 | 10 | 96 |
| safe_pool_bench | 2 | 111 | 1664 | 10 | 96 |
| safe_pool_bench | 3 | 109 | 1792 | 9 | 96 |
| smart_bench | 1 | 1348 | 1792 | 134 | 96 |
| smart_bench | 2 | 1442 | 1792 | 143 | 96 |
| smart_bench | 3 | 1436 | 1664 | 142 | 96 |
