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

/* STRUCT interop (v0.48.1 generics engine) */
tx_t ffi_make_point(long x, long y);
tx_t ffi_point_sum(tx_t p);
tx_t ffi_make_box(tx_t v);
void ffi_box_write(tx_t b, long v);
tx_t ffi_box_read(tx_t b);
tx_t ffi_make_pair(tx_t a, tx_t b);
tx_t ffi_pair_read(tx_t p);
tx_t ffi_make_wrap(tx_t box, tx_t tag);
tx_t ffi_w_read(tx_t w);

/* v0.48.4 FFI-extension errno reader */
long ffi_ffi_errno(void);

/* v0.48.12 ENUM FFI (tests declare ENUM Color + these externals) */
tx_t ffi_color(void);
tx_t ffi_is_green(tx_t c);
tx_t ffi_color_idx(tx_t c);

/* v0.48.14 Async IN Context (wraps the runtime context API) */
tx_t ffi_ctx_make(long adaptive, long cap, tx_t name);
tx_t ffi_ctx_tasks(tx_t ctx);
tx_t ffi_read_trace(tx_t path);

#endif
