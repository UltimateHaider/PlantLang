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

/* ── STRUCT interop: opaque tx_t handles around C structs ──────
   The generated C emits typedefs (plant_Point, plant_Box_NUM, …)
   for the FFI side; the mock keeps its own layout twins so the
   test binary never sees duplicate typedef definitions. */

typedef struct { long x; long y; } MockPoint;
typedef struct { tx_t val; }      MockBox;
typedef struct { tx_t first, second; } MockPair;
typedef struct { tx_t box, tag; } MockWrap;

/* non-generic STRUCT Point { x: NUM, y: NUM } */
tx_t ffi_make_point(long x, long y) {
    MockPoint* p = (MockPoint*)malloc(sizeof(MockPoint));
    if (!p) return "";
    p->x = x; p->y = y;
    return (tx_t)p;
}

tx_t ffi_point_sum(tx_t p) {
    MockPoint* mp = (MockPoint*)p;
    if (!mp) return _from_long(0);
    return _from_long(mp->x + mp->y);
}

/* generic STRUCT Box[T] { val: T } — NUM instantiation */
tx_t ffi_make_box(tx_t v) {
    MockBox* b = (MockBox*)malloc(sizeof(MockBox));
    if (!b) return "";
    b->val = v;
    return (tx_t)b;
}

void ffi_box_write(tx_t b, long v) {
    MockBox* mb = (MockBox*)b;
    if (!mb) return;
    mb->val = _from_long(v);
}

tx_t ffi_box_read(tx_t b) {
    MockBox* mb = (MockBox*)b;
    if (!mb) return "";
    return mb->val;
}

/* generic STRUCT Pair[T, U] { first: T, second: U } */
tx_t ffi_make_pair(tx_t a, tx_t b) {
    MockPair* p = (MockPair*)malloc(sizeof(MockPair));
    if (!p) return "";
    p->first = a; p->second = b;
    return (tx_t)p;
}

tx_t ffi_pair_read(tx_t p) {
    MockPair* mp = (MockPair*)p;
    if (!mp) return "";
    return _cat(_cat(_S(mp->first), ":"), _S(mp->second));
}

/* nested generic STRUCT Wrap[T] { box: Box[T], tag: TX } */
tx_t ffi_make_wrap(tx_t box, tx_t tag) {
    MockWrap* w = (MockWrap*)malloc(sizeof(MockWrap));
    if (!w) return "";
    w->box = box; w->tag = tag;
    return (tx_t)w;
}

tx_t ffi_w_read(tx_t w) {
    MockWrap* mw = (MockWrap*)w;
    if (!mw) return "";
    MockBox* mb = (MockBox*)mw->box;
    return mb ? mb->val : (tx_t)"";
}
