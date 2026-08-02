# Performance Results

- Generated: 2026-08-02 01:58 UTC
- Toolchain: Linux 6.6.76-08111-g8df27f55632a x86_64
- Compiler:  Chloroplast 0.48.3a (pure native)
- Host CC:   gcc (Debian 12.2.0-14+deb12u1) 12.2.0
- Method: 3 runs per benchmark; real time via `date +%s%N`, peak RSS polled from /proc/PID/status (VmHWM).

| benchmark | run | real (ms) | peak RSS (KB) | CPU ticks | exit |
|---|---|---|---|---|---|
| perf_async | 1 | 6 | 0 | 0 | 64 |
| perf_async | 2 | 6 | 0 | 0 | 64 |
| perf_async | 3 | 7 | 0 | 0 | 64 |
| perf_concat | 1 | 140 | 206080 | 12 | 64 |
| perf_concat | 2 | 131 | 206080 | 12 | 64 |
| perf_concat | 3 | 124 | 199296 | 11 | 64 |
| perf_mixed | 1 | 163 | 299776 | 15 | 64 |
| perf_mixed | 2 | 174 | 290560 | 15 | 64 |
| perf_mixed | 3 | 185 | 290560 | 16 | 64 |
