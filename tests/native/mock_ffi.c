/*
 * Mock FFI library for the v0.47.3 advanced-FFI integration tests.
 * Simulates the ABI of a shared C library: plain TX returns,
 * pass-by-reference (REF) params, Result<T,E>-style error returns
 * via errno, and malloc'd buffers for the ffi_free lifecycle.
 * Linked directly into test binaries alongside plant_runtime.c.
 */
#include <plant_compat.h>
#include <errno.h>
#include <stdarg.h>
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

/* ── v0.48.4 FFI Optional Extensions ─────────────────────────────
   By-value struct params, REF STRUCT pointers, STRUCT returns,
   void*, CALLBACK lifecycle, variadic calls. The generated types
   header (-include) provides plant_Point & co + the prototypes,
   so these definitions only compile for tests that declare the
   matching feature macros. */

#ifndef MOCK_FFI_EXT_H
#define MOCK_FFI_EXT_H
#ifdef PLANT_STRUCT_plant_Point

tx_t ffi_point_sumv(plant_Point p) {
    return _from_long(p.x + p.y);
}

tx_t ffi_scale_ref(plant_Point* p, long k) {
    if (!p) return _from_long(0);
    return _from_long(p->x * k);
}

plant_Point ffi_get_point(void) {
    plant_Point p;
    memset(&p, 0, sizeof(p));
    p.x = 7;
    p.y = 9;
    return p;
}

#endif /* PLANT_STRUCT_plant_Point */

#ifdef PLANT_STRUCT_plant_Ball

plant_Ball ffi_get_ball(void) {
    plant_Ball p;
    memset(&p, 0, sizeof(p));
    p.weight = 5;
    p.color = GREEN;
    return p;
}

#endif /* PLANT_STRUCT_plant_Ball */

#ifdef PLANT_STRUCT_plant_Game

plant_Game ffi_get_game(void) {
    plant_Game g;
    memset(&g, 0, sizeof(g));
    g.state = DONE;
    g.color = BLUE;
    return g;
}

#endif /* PLANT_STRUCT_plant_Game */

#ifdef PLANT_STRUCT_plant_WrapV

tx_t ffi_wrap_sumv(plant_WrapV w) {
    return _from_long(w.p.x + w.p.y);
}

#endif /* PLANT_STRUCT_plant_WrapV */

#ifdef PLANT_STRUCT_plant_L4

tx_t ffi_l4_v(plant_L4 v) {
    return _from_long(v.a.a.a.v);
}

#endif /* PLANT_STRUCT_plant_L4 */

#ifdef PLANT_FFI_HAS_CALLBACKS

tx_t ffi_run_cb(plant_cb_t cb, long ctx, tx_t val) {
    return plant_cb_call((tx_t)cb, ctx, val);
}

#endif /* PLANT_FFI_HAS_CALLBACKS */

#ifdef PLANT_FFI_HAS_VOIDPTR

tx_t ffi_buf_lenv(void* b) {
    if (!b) return _from_long(0);
    return _from_long((long)strlen((const char*)b));
}

void* ffi_get_buf(void) {
    char* b = (char*)malloc(5);
    if (!b) return NULL;
    memcpy(b, "buf!", 5);
    return b;
}

#endif /* PLANT_FFI_HAS_VOIDPTR */

#ifdef PLANT_FFI_HAS_VARARGS

tx_t ffi_vprintf(tx_t fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const char* f = _S(fmt);
    char out[1024];
    size_t o = 0;
    for (const char* p = f; *p && o + 1 < sizeof(out); p++) {
        if (*p == '%' && p[1]) {
            p++;
            if (*p == 'd') {
                long v = (long)(intptr_t)va_arg(ap, tx_t);
                o += (size_t)snprintf(out + o, sizeof(out) - o, "%ld", v);
            } else if (*p == 's') {
                const char* s = _S(va_arg(ap, tx_t));
                o += (size_t)snprintf(out + o, sizeof(out) - o, "%s", s ? s : "");
            } else {
                out[o++] = '%';
                out[o++] = *p;
            }
        } else {
            out[o++] = *p;
        }
    }
    va_end(ap);
    out[o] = 0;
    return strdup(out);
}

#endif /* PLANT_FFI_HAS_VARARGS */

/* FFI-extension errno reader (plain external, always available) */
long ffi_ffi_errno(void) {
    return plant_ffi_errno;
}

/* ── v0.48.12 ENUM FFI ─────────────────────────────────────────
   External functions that take/return ENUM Color values. The C
   side works with raw member ints (RED=0, GREEN=1, BLUE=2); the
   generated code marshals with _to_enum (params) and _from_enum
   (returns). Guarded by the PLANT_ENUM_Color macro emitted into
   the generated types header by the enum_decl codegen. */

#ifdef PLANT_ENUM_Color

/* ffi_color() -> ENUM Color: raw member index of GREEN */
tx_t ffi_color(void) {
    return (tx_t)1;
}

/* ffi_is_green(c(ENUM Color)) -> external: 1 when c is GREEN */
tx_t ffi_is_green(tx_t c) {
    return _from_long((long)c == 1);
}

/* ffi_color_idx(c(ENUM Color)) -> external: the raw member int */
tx_t ffi_color_idx(tx_t c) {
    return _from_long((long)c);
}

#endif /* PLANT_ENUM_Color */

#endif /* MOCK_FFI_EXT_H */
