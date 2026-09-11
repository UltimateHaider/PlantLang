/*
 * Benchmark FFI shim — monotonic millisecond clock for intra-program
 * phase timing (ffi_overhead.plant). Linked by benchmark.sh only.
 */
#include <plant_compat.h>
#include <time.h>

tx_t bench_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return _from_long((long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
}

/* minimal external call target (no return value, no allocation) */
void ffi_noop(long v) {
    (void)v;
}
