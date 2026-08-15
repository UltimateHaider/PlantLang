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

/* ── v0.48.14 Async IN Context ───────────────────────────────
   ffi_ctx_make: create a named execution context (wraps the
   runtime plant_async_ctx_create; a tx_t handle).
   ffi_ctx_tasks: live task count inside the context.
   ffi_read_trace: read a trace file, stripping the "T," type tag
   and the nondeterministic ms timestamp so regression expected
   output stays deterministic ("<scope>,<level>,<msg>"). */
tx_t ffi_ctx_make(long adaptive, long cap, tx_t name) {
    return plant_async_ctx_create(adaptive, cap, name);
}

tx_t ffi_ctx_tasks(tx_t ctx) {
  return _from_long(plant_async_ctx_tasks(ctx));
}

tx_t ffi_read_trace(tx_t path) {
    FILE* f = fopen(_S(path), "r");
    if (!f) return strdup("");
    static char out[4096];
    size_t used = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && used < sizeof(out) - 1) {
        char* p = line;
        if (strncmp(p, "T,", 2) == 0) p += 2;       /* strip type tag */
        char* c1 = strchr(p, ',');
        if (c1) p = c1 + 1;                          /* strip ms field */
        used += (size_t)snprintf(out + used, sizeof(out) - used, "%s", p);
    }
    fclose(f);
    if (used > 0 && out[used - 1] == '\n') out[--used] = 0;
    return strdup(out);
}

/* ── v0.48.15 Mission Mode FAST wrappers (bump heap + audit) ──
   ffi_fast_alloc returns the bump pointer as a tx_t handle (never
   dereferenced from PlantLang); counts/status come back as text. */
tx_t ffi_fast_alloc(tx_t n) {
    return plant_fast_alloc(n);
}

tx_t ffi_fast_reset(void) {
    return plant_fast_reset();
}

tx_t ffi_fast_used(void) {
    return plant_fast_used();
}

tx_t ffi_fast_peak(void) {
    return plant_fast_peak();
}

tx_t ffi_fast_escalated(void) {
    return plant_fast_escalated();
}

tx_t ffi_fast_status(void) {
    return plant_fast_status();
}

tx_t ffi_audit_dump(void) {
    return plant_audit_dump();
}

tx_t ffi_cap_check(tx_t cap) {
    return plant_cap_check(cap);
}

/* ── v0.48.16 Mission Mode SAFE wrappers (warm pool + SafeChannel +
   hash-chained audit). The pool/channel functions live in the runtime;
   these wrappers also expose test-only fault injection (stall, starve,
   tamper) so the heartbeat/starvation/audit tests are deterministic. */
tx_t ffi_safe_status(void) {
    return plant_safe_status();
}

tx_t ffi_safe_stall(tx_t namev) {
    return plant_safe_stall(namev);
}

tx_t ffi_safe_heartbeat_tick(void) {
    return _from_long(plant_pool_tick());
}

tx_t ffi_safe_starve(tx_t ms) {
    return plant_safe_starve(ms);
}

tx_t ffi_safe_grant(tx_t cap) {
    return plant_safe_grant(cap);
}

tx_t ffi_safe_syscall(tx_t name) {
    return plant_syscall_check(name);
}

tx_t ffi_safe_channel_open(void) {
    return plant_safe_channel_open();
}

tx_t ffi_safe_send(tx_t chan, tx_t payload) {
    return plant_safe_send(chan, payload);
}

tx_t ffi_safe_send_big(tx_t chan, tx_t n) {
    return plant_safe_send_big(chan, n);
}

tx_t ffi_safe_recv(tx_t chan) {
    return plant_safe_recv(chan);
}

tx_t ffi_safe_stats(tx_t chan) {
    return plant_safe_stats(chan);
}

tx_t ffi_audit_chain_verify(void) {
    return plant_audit_chain_verify();
}

tx_t ffi_audit_chain_head(void) {
    return plant_audit_chain_head();
}

tx_t ffi_audit_tamper(void) {
    return plant_audit_tamper();
}

/* v0.48.17 Mission Mode SMART (SmartExecutionRouter vec pool) */
tx_t ffi_smart_status(void) {
    return plant_smart_status();
}

/* v0.48.18 Mission Mode PERSISTENT (GlobalARCHeap) */
tx_t ffi_arc_alloc(tx_t size) {
    return plant_arc_alloc(size);
}
tx_t ffi_arc_retain(tx_t obj) {
    return plant_arc_retain(obj);
}
tx_t ffi_arc_release(tx_t obj) {
    return plant_arc_release(obj);
}
tx_t ffi_arc_link(tx_t parent, tx_t child) {
    return plant_arc_link(parent, child);
}
tx_t ffi_arc_unlink(tx_t parent, tx_t child) {
    return plant_arc_unlink(parent, child);
}
tx_t ffi_arc_lease(tx_t obj, tx_t ms) {
    return plant_arc_lease(obj, ms);
}
tx_t ffi_arc_set_finalizer(tx_t obj, tx_t name) {
    return plant_arc_set_finalizer(obj, name);
}
tx_t ffi_arc_persist(tx_t obj) {
    return plant_arc_persist(obj);
}
tx_t ffi_arc_gc(void) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld", plant_arc_gc());
    return buf;
}
tx_t ffi_arc_finalized(void) {
    return plant_arc_finalize_count();
}
tx_t ffi_persist_status(void) {
    return plant_persist_status();
}
tx_t ffi_lease_evict(void) {
    return _from_long(plant_lease_evict());
}
tx_t ffi_persist_pressure(void) {
    return _from_long(plant_persist_pressure());
}
tx_t ffi_sleep(tx_t ms) {
    plant_msleep((long)ms);
    return (tx_t)"0";
}

/* v0.48.37d Weather memory management + diagnostics */
tx_t ffi_weather_register(tx_t handle) {
    return _from_long(plant_weather_register_handle(handle));
}
tx_t ffi_weather_defer(tx_t handle) {
    return _from_long(plant_weather_defer_handle(handle));
}
tx_t ffi_weather_status(void) {
    return plant_weather_status();
}

/* ── v0.48.37 Memory Safety Layer wrappers (slabs, FREE, DIST) ── */
tx_t ffi_mem_free(tx_t v) {
    return plant_mem_free(v);
}
tx_t ffi_mem_report(void) {
    return plant_mem_report();
}
tx_t ffi_mem_scan(void) {
    return plant_mem_scan();
}
tx_t ffi_dist_init(tx_t nodes) {
    return plant_dist_init(nodes);
}
tx_t ffi_dist_alloc(tx_t size, tx_t key) {
    return plant_dist_alloc(size, key);
}
tx_t ffi_dist_node(tx_t obj) {
    return plant_dist_node(obj);
}
tx_t ffi_dist_release(tx_t obj) {
    return plant_dist_release(obj);
}
tx_t ffi_dist_status(void) {
    return plant_dist_status();
}

/* ── v0.48.37c SAFE real-process isolation test helpers ── */
tx_t ffi_make_big(tx_t nv) {
    long n = (long)nv;
    if (n < 1) n = 1;
    if (n > (1L << 24)) n = (1L << 24);
    char* s = malloc((size_t)n + 1);
    if (!s) return (tx_t)"";
    for (long i = 0; i < n; i++) s[i] = (char)('a' + (i % 26));
    s[n] = 0;
    return s;
}
tx_t ffi_big_ok(tx_t sv, tx_t nv) {
    const char* s = _S(sv);
    long n = (long)nv;
    if (!s || (long)strlen(s) != n) return (tx_t)"0";
    for (long i = 0; i < n; i++)
        if (s[i] != (char)('a' + (i % 26))) return (tx_t)"0";
    return (tx_t)"1";
}
tx_t ffi_str_len(tx_t sv) {
    const char* s = _S(sv);
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld", s ? (long)strlen(s) : 0);
    return buf;
}
tx_t ffi_str_eq(tx_t a, tx_t b) {
    const char* x = _S(a);
    const char* y = _S(b);
    if (!x || !y) return (x == y) ? (tx_t)"1" : (tx_t)"0";
    return (strcmp(x, y) == 0) ? (tx_t)"1" : (tx_t)"0";
}
tx_t ffi_list_count(tx_t lv) {
    PlantArray* p = (PlantArray*)lv;
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld",
             (p && p->magic == PLANT_ARRAY_MAGIC) ? (long)p->count : 0);
    return buf;
}
tx_t ffi_list_get(tx_t lv, tx_t iv) {
    PlantArray* p = (PlantArray*)lv;
    long i = (long)iv;
    if (!p || p->magic != PLANT_ARRAY_MAGIC) return NULL;
    if (i < 0 || i >= (long)p->count) return NULL;
    return p->items[i];
}

#endif /* MOCK_FFI_EXT_H */
