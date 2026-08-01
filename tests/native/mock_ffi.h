/*
 * Prototypes for the v0.47.3 mock FFI library (mock_ffi.c).
 * The native test runner force-includes this header into every
 * test TU (-include), so the generated C sees the real ABI
 * signatures instead of implicit-int declarations.
 */
#ifndef MOCK_FFI_H
#define MOCK_FFI_H

#include <plant_compat.h>

tx_t ffi_add(long a, long b);
void ffi_swap_ref(long* a, long* b);
tx_t ffi_make_buf(long n);
tx_t ffi_open_mock(long mode);
long ffi_parse_cfg(tx_t path);

#endif
