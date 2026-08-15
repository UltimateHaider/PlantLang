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

/* v0.48.15 Mission Mode FAST (wraps the runtime bump heap + audit) */
tx_t ffi_fast_alloc(tx_t n);
tx_t ffi_fast_reset(void);
tx_t ffi_fast_used(void);
tx_t ffi_fast_peak(void);
tx_t ffi_fast_escalated(void);
tx_t ffi_fast_status(void);
tx_t ffi_audit_dump(void);
tx_t ffi_cap_check(tx_t cap);
tx_t ffi_safe_status(void);
tx_t ffi_safe_stall(tx_t name);
tx_t ffi_safe_heartbeat_tick(void);
tx_t ffi_safe_starve(tx_t ms);
tx_t ffi_safe_grant(tx_t cap);
tx_t ffi_safe_syscall(tx_t name);
tx_t ffi_safe_channel_open(void);
tx_t ffi_safe_send(tx_t chan, tx_t payload);
tx_t ffi_safe_send_big(tx_t chan, tx_t n);
tx_t ffi_safe_recv(tx_t chan);
tx_t ffi_safe_stats(tx_t chan);
tx_t ffi_audit_chain_verify(void);
tx_t ffi_audit_chain_head(void);
tx_t ffi_audit_tamper(void);
tx_t ffi_smart_status(void);
tx_t ffi_arc_alloc(tx_t size);
tx_t ffi_arc_retain(tx_t obj);
tx_t ffi_arc_release(tx_t obj);
tx_t ffi_arc_link(tx_t parent, tx_t child);
tx_t ffi_arc_unlink(tx_t parent, tx_t child);
tx_t ffi_arc_lease(tx_t obj, tx_t ms);
tx_t ffi_arc_set_finalizer(tx_t obj, tx_t name);
tx_t ffi_arc_persist(tx_t obj);
tx_t ffi_arc_gc(void);
tx_t ffi_arc_finalized(void);
tx_t ffi_persist_status(void);
tx_t ffi_lease_evict(void);          /* v0.48.37b */
tx_t ffi_persist_pressure(void);     /* v0.48.37b */
tx_t ffi_sleep(tx_t ms);

/* v0.48.37 Memory Safety Layer (slabs, FREE, DistributedHeap) */
tx_t ffi_mem_free(tx_t v);
tx_t ffi_mem_report(void);
tx_t ffi_mem_scan(void);
tx_t ffi_dist_init(tx_t nodes);
tx_t ffi_dist_alloc(tx_t size, tx_t key);
tx_t ffi_dist_node(tx_t obj);
tx_t ffi_dist_release(tx_t obj);
tx_t ffi_dist_status(void);

/* v0.48.37c SAFE real-process isolation test helpers */
tx_t ffi_make_big(tx_t n);
tx_t ffi_big_ok(tx_t s, tx_t n);
tx_t ffi_str_len(tx_t s);
tx_t ffi_str_eq(tx_t a, tx_t b);
tx_t ffi_list_count(tx_t l);
tx_t ffi_list_get(tx_t l, tx_t i);

#endif
