/*
 * Mock FFI library for the v0.47.3 advanced-FFI integration tests.
 * Simulates the ABI of a shared C library: plain TX returns,
 * pass-by-reference (REF) params, Result<T,E>-style error returns
 * via errno, and malloc'd buffers for the ffi_free lifecycle.
 * Linked directly into test binaries alongside plant_runtime.c.
 */
#include <plant_compat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* plain TX-returning call: ffi_add(2, 3) -> "5" */
tx_t ffi_add(long a, long b) {
    return _from_long(a + b);
}

/* pass-by-reference demo: swaps two longs through pointers */
void ffi_swap_ref(long* a, long* b) {
    long t = *a;
    *a = *b;
    *b = t;
}

/* allocates a zero-terminated buffer of n 'x' bytes for ffi_free */
tx_t ffi_make_buf(long n) {
    if (n < 1) { errno = EINVAL; return ""; }
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { errno = ENOMEM; return ""; }
    memset(b, 'x', (size_t)n);
    b[n] = 0;
    return (tx_t)b;
}

/* simulated open(): Result<NUM,TX> semantics — fails with ENOENT
   when mode == 0, otherwise succeeds with handle 42 */
tx_t ffi_open_mock(long mode) {
    if (mode == 0) { errno = ENOENT; return ""; }
    errno = 0;
    return _from_long(42);
}

/* Result<NUM,TX> semantics: returns -1 + errno=EINVAL unless
   path == "ok", which returns 7 */
long ffi_parse_cfg(tx_t path) {
    if (!path || strcmp(_S(path), "ok") != 0) { errno = EINVAL; return -1; }
    errno = 0;
    return 7;
}
