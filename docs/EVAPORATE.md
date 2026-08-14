# EVAPORATE — Memory Management in Chloroplast

**Status:** authoritative reference · **Verified against:** `runtime/c/plant_runtime.c` (4653 lines), `runtime/c/plant_compat.h`, `runtime/c/plant_runtime.h`, `src/plantc/codegen_c.plant` (5957 lines), `make self` (converged, 351499 bytes), `make test` (native 20, generics 7, closures 6, regression 111 — all green).

> **Naming note.** EVAPORATE is the *documentation brand* for Chloroplast's memory philosophy, adopted here as the organizing convention. The codebase itself does not use the word "evaporate"; every mechanism described below is real and is cited by file and line. Where the EVAPORATE model implies a primitive that does not exist (for example `plant_arena_reset`), this document says so explicitly and names the mechanism that actually performs the equivalent job.

---

## 1. Executive Summary

Chloroplast compiles to plain C. Every script value crosses boundaries as `tx_t` — an opaque `void*` (`runtime/c/plant_types.h:12`) that is either a heap C string, a `PlantArray`/`PlantMap` container pointer, or raw integer bits stored in the pointer slot (never dereferenced).

The memory philosophy is:

- **Deterministic.** Allocation is explicit at the point of call; nothing is reclaimed by a background collector unless a mission mode turns one on. There is no free-floating garbage collector in the default path.
- **Scope-based.** "Memory evaporates when its scope evaporates." The engine provides three real scope-lifetime mechanisms: per-task **segmented arenas** (async tasks), a **bump heap** (FAST mission mode), and a **ref-counted heap with tri-color cycle detection** (PERSISTENT mission mode). Each resets or frees at a well-defined scope boundary.
- **Zero-overhead default.** The BALANCED default is plain `malloc`/`strdup` — no headers beyond the C library, no bookkeeping, no finalizers. The compiler pays exactly for what it uses; a 64KB or 8MB allocation block is the C allocator's business, not the runtime's.

**Why the name fits.** In the mission modes, memory is deliberately short-lived: FAST allocations evaporate when the action's bump pointer rewinds; arena segments evaporate when the task tree is torn down; ARC objects evaporate the instant their reference count reaches zero (or their lease lapses). A well-formed Chloroplast program is one in which nothing outlives its owning scope — everything *evaporates*.

**The honest baseline.** Outside the mission modes and the async engine, BALANCED-mode strings and aggregates are *not* reclaimed automatically. The generated C is straight-line C; a `tx_t` variable is a raw pointer, and there is no language-level `free`. Short-lived programs (the compiler itself included) simply return everything to the OS at exit. This is the primary documented gap (Section 7).

---

## 2. The Rooted Depth System (Core Architecture)

### 2.1 What exists: the async task arena tree

The closest thing to a "rooted depth system" is the async engine's **task-lifetime arena model** (`runtime/c/plant_runtime.c:2662–2866`). Every generated `ASYNC ACTION` is a state struct allocated inside its own `plant_arena` (`plant_async_alloc_state`, line 3132); child tasks hang off the parent's task tree (`t->chd`/`csib`), and each task owns exactly one arena.

```
struct plant_task {
    ...
    plant_arena  ar;          /* this task's arena  (line 2733) */
    tx_t         st;          /* state struct, arena-allocated (line 2716) */
    ...
}
```

### 2.2 Arena slabs

Segments (`plant_seg`, line 2676) are variable-size blocks: `{ cap, used, refs, next }` with the payload immediately after the header. There is **no fixed 64KB slab in the current runtime** — segment size is *adaptive*:

- initial global `g_seg_size = 1024` bytes (line 2766);
- per-arena `seg_size` doubles while the allocation miss ratio exceeds 8%, halves below 2%, clamped to `256 B … 1 MB` (lines 2845–2849);
- a global segment cache (`g_seg_cache`, `g_cache_max = 64`, lines 2767, 2797–2803) recycles freed segments instead of returning them to the OS.

(The 64KB slab arena from the EVAPORATE model does exist — as the *legacy* standalone runtime's `ArenaSlab` with `ARENA_DEFAULT_SLAB_SIZE = 65536`, `runtime/runtime.c:24–93` — but that file is not part of the self-hosted pipeline.)

### 2.3 Bump-pointer allocation — `plant_arena_alloc`

`plant_arena_alloc` (line 2784) scans the arena's segments for one with `used + n <= cap`, bumps `used`, and returns the pointer. On a miss it takes a cached segment from `g_seg_cache` or `malloc`s a new one. Allocation is O(segments) with ~1 hit on the steady state; hits/misses are counted (`g_arena_hits`, `g_arena_misses`) and feed the MetricsAggregator's `miss%` sample (lines 3111–3112).

### 2.4 Automatic reset on scope exit — *reality check*

There is **no `plant_arena_reset`**. Task arenas are not reset; they are **freed whole** at teardown:

- `plant_arena_free` (line 2844) decrements each segment's refcount and either returns it to `g_seg_cache` or `free`s it;
- `plant_task_free` calls it once per task (line 3043);
- `plant_teardown_tree` / `plant_teardown_children` (lines 3048–3067) walk the task tree and free every descendant exactly once;
- `plant_cancel_task` (line 3072) frees the cancelled subtree's arenas immediately.

The *scope-exit reset* role of `plant_arena_reset` is instead played by:

- **FAST mode**: `plant_fast_enter` rewinds the bump pointer at every action entry (Section 3.2);
- **PERSISTENT mode**: `plant_arc_release` destroys objects at refs=0 (Section 3.3).

### 2.5 Depth tracking

Two real depth mechanisms exist:

1. **Mission-mode stack** — `g_mode_stack[64]`, `plant_mode_push`/`pop` (lines 3479–3489). The innermost active mission (BALANCED, `F`, `S`, `M`, `P`) governs capability checks (`plant_boundary_block`, line 3720) and SAFE-taint marking of ARC objects (`plant_arc_in_safe`, line 4402).
2. **Async task tree** — parent/child `chd`/`csib` links; teardown is depth-first.

### 2.6 Interaction with control flow

- **GIVE / return**: mission exit lines (`plant_fast_exit`, `plant_safe_exit`, `plant_smart_exit`, `plant_persist_exit`) are computed up front and threaded through the body generator so every `GIVE` emits the exit before `return` (`src/plantc/codegen_c.plant:3269–3278`).
- **BREAK / CONTINUE**: straight-line C — no cleanup hooks needed for scoped bump/arena memory.
- **WEATHER / SHELTER / CALM**: `setjmp`/`longjmp` frames (`PlantWeather`, `plant_runtime.h:86–103`). Arena and bump memory is *not* unwound by `longjmp`; only the mission exit lines on the normal path apply. Long-running CALM handlers should treat mission-mode memory as still owned.

---

## 3. Memory Allocation Primitives

### 3.1 BALANCED (default) — plain heap

| Primitive | Location | Behaviour |
|---|---|---|
| `plant_alloc` | `plant_runtime.c:54` | `malloc`, **exit(1) on OOM** with a diagnostic |
| `plant_free` | `plant_runtime.c:60` | `free` |
| `plant_str_concat` | `plant_runtime.c:66` | `malloc` + copy |
| `_cat` / `_cat3` / `_cat4` | `plant_compat.h:22–27` | one `malloc` per concatenation group; NULL coerces to `""`; falls back to first arg on OOM. `_cat3/_cat4` flatten chains (one alloc for 3–4 segments) |
| `_from_digit` | `plant_compat.h:34` | single-digit (0–9) static table — **zero allocation**; larger values `snprintf` + `strdup` |
| `_from_long` | `plant_compat.h:35` | funnels through `_from_digit` |
| `strdup` | everywhere | canonical string copy in the runtime |

`tx_t` strings are never written into after creation — which is what makes the `_from_digit` static table safe (comment at `plant_compat.h:29–33`).

### 3.2 FAST — bump allocator

`plant_fast_heap` (`plant_runtime.c:3601–3611`): `{ base, size, used, peak, limit, alignment }`. Defaults: **8 MB capacity, 64 MB hard limit, strict 8-byte alignment** (`plant_fast_init`, lines 3618–3626), overridable per program:

```
MISSION CONFIG FAST_HEAP_CAPACITY = 256.   /* >= 64 bytes  */
MISSION CONFIG FAST_HEAP_LIMIT    = 512.
MISSION CONFIG FAST_ALIGNMENT     = 4.
```

- `plant_fast_alloc_raw` (line 3638): aligned bump; `plant_fast_grow` doubles up to the limit (line 3628); beyond that, **escalation** to `malloc` — recorded once in the audit ring (`FAST_ESCALATE`), and freed only by `plant_fast_reset` (line 3678), max 256 escaped blocks.
- `plant_fast_alloc` (line 3674) is the language-visible entry point (`plant_fast_alloc(64)` in PlantLang).
- Codegen: every `ACTION … WITH MISSION FAST` emits the boundary guard + `plant_fast_enter(name)` at entry (`codegen_c.plant:3287–3288`) and `plant_fast_exit` on all exits (line 3275). `plant_fast_enter` **resets the bump pointer at every scope enter** (line 3662–3666) — this is the EVAPORATE "reset on scope exit" behaviour, executed lazily at the next enter.
- Introspection: `plant_fast_used` / `plant_fast_peak` / `plant_fast_escalated` / `plant_fast_status` (lines 3687–3709).

### 3.3 PERSISTENT — ARC heap

`plant_arc_obj` (`plant_runtime.c:4367–4379`): `{ data, size, refs, in_edges, mark, alloc_seq, leased_until_ms, tainted, finalizer, edges, next }`, managed on the global `g_arc_head` list.

| Primitive | Location | Behaviour |
|---|---|---|
| `plant_arc_alloc(sz)` | 4452 | `calloc` object + `calloc` payload, `refs=1`; triggers automatic cycle detection every `PERSIST_GC_INTERVAL` (default 1000, line 3554) allocations |
| `plant_arc_retain(obj)` | 4475 | `refs++` |
| `plant_arc_release(obj)` | 4485 | `refs--`; at 0 → `plant_arc_drop` (4440) which honours an unexpired lease, else destroys |
| `plant_arc_link(p, c)` | 4501 | records an edge, `c->refs++` (storing a reference retains) |
| `plant_arc_unlink(p, c)` | 4518 | drops the edge, `refs--`, destroys the child at 0 |
| `plant_arc_lease(obj, ms)` | 4542 | keeps a refs=0 object alive past release (persistent cache path) |
| `plant_arc_set_finalizer(obj, "name")` | 4555 | registers a named finalizer (`free_data`, `close_ctx`; table at 4395–4400) |
| `plant_arc_persist(obj)` | 4573 | integrity gate: objects allocated while SAFE is on the mode stack are **tainted** and refused |
| `plant_arc_gc()` | 4599 | tri-color mark-sweep: roots are objects with `refs − in_edges > 0`; unmarked or dead-lease objects are destroyed with finalizers |

### 3.4 Aggregates

- **Lists** — `PlantArray` (`plant_runtime.h:108–119`): `{ magic = 0x504C4152 "PLAR", count, capacity, items }`. `plant_list_create` (1083), `plant_list_push` grows via `realloc` doubling from 8 (1111), `plant_list_make(count, ...)` (1122) — **count must equal the argument count** (a past mismatch caused a compiler segfault; fixed in v0.48.35).
- **Maps** — `PlantMap` (`plant_runtime.h:37–48`): open addressing, **75% load factor**; `plant_map_set` `strdup`s keys (737); `_plant_map_grow` doubles capacity and frees the old key copies (712–728); `plant_map_free` frees every key, the entry table, and the map (775–782).
- **Marshalled structs** — `plant_struct_free` (2657) is `free()` of an FFI-marshalled copy.

### 3.5 `plant_alloc` / `plant_free` — general-purpose heap

Covered in 3.1. These are the raw `malloc`/`free` wrappers used by every container (list, map, network maps, JSON trees, ARC edge nodes). **Note:** the language surface exposes no statement that reaches `plant_free` — it is used internally by the runtime only.

---

## 4. Memory Deallocation & Cleanup

### 4.1 Arena reset → task teardown

The EVAPORATE "reset on scope exit" for async state is whole-arena teardown: `plant_arena_free` (2844) → segment refcounts → cache or `free`; run exactly once per task via `plant_teardown_tree` / `plant_teardown_children` / `plant_cancel_task` (3048–3096).

### 4.2 Fast heap reset

`plant_fast_reset` (3678): records the peak, rewinds `used = 0`, frees all escalated `malloc` escapes. Scope semantics: `plant_fast_enter` (3662) resets at the next action entry, so each FAST action sees a fresh heap.

### 4.3 ARC release

`plant_arc_release` → `plant_arc_drop` (4440) → `plant_arc_destroy` (4408): runs the registered finalizer (audit `ARC_FINALIZE`), frees every outgoing edge node, the payload, and the object (audit `ARC_FREE`). Cycle reclamation is `plant_arc_gc` (4599), run automatically every 1000 allocations and on demand; reclaimed cycles run their finalizers too. Expired zero-ref leases are reclaimed by GC as well (line 4609).

### 4.4 Explicit cleanup helpers

The runtime exposes `plant_map_free` (775), `plant_list_*` (no whole-list free — list item ownership is caller-defined; the list struct itself is freed with `free` via `plant_free`), and `plant_struct_free` (2657). **There are no language statements that invoke any of these.**

### 4.5 Process exit

- `plant_async_drain` (4313): runs the dispatcher until `g_live == 0` — firing timers, stepping tasks — then flushes the trace file. Every completed task's arena and state were freed at `plant_async_finish`/teardown.
- Remaining BALANCED heap (compiler scratch strings, closures, `PlantArray`s) is **not** walked at exit; the process returns it to the OS. The generated `main` is `plant_init_cli → body → plant_async_drain → return 0` (`codegen_c.plant:5363`).

---

## 5. Mission-Specific Memory Behaviors

| Mission | Memory behavior | Where |
|---|---|---|
| **BALANCED** (default) | plain `malloc`/`strdup`; nothing reclaimed mid-program | §3.1 |
| **FAST** | bump heap, O(1) aligned allocation, zero fragmentation, rewind per scope enter, malloc escalation on overflow | §3.2 |
| **PERSISTENT** | ARC heap: refs, edges, leases, finalizers; tri-color GC every 1000 allocs reclaims cycles | §3.3 |
| **SAFE** | WarmProcessPool — in-process worker emulation (no real process isolation, no shared memory): heartbeat monitoring, stall/restart, starvation growth, BALANCED fallback; SafeChannel payload freed at `plant_safe_exit` (3855) | §5.1 |
| **SMART** | SmartExecutionRouter — scalar inline below 1000 elements, else Parallel Vector Mode: chunks of 256 dispatched over a CPU-core-sized vec pool (in-process emulation), pool expansion and BALANCED fallback under queue pressure | §5.2 |

### 5.1 SAFE — WarmProcessPool

`plant_worker` (3495–3507): `{ name, state, last_heartbeat_ms, spawn_seq, busy_since_ms, served_calls, chan }`; fixed array `g_pool[16]` (3517), default capacity 4 (3519). `plant_pool_tick` (3772) restarts stalled workers; `plant_pool_acquire` (3801) grows under starvation (thresholds 5000/10/50 ms, lines 3528–3530) then falls back to the inline BALANCED worker (3527). Memory is per-worker channel buffers, freed at exit. SAFE also zeroes capabilities and taints ARC objects.

### 5.2 SMART — vector pool

`g_vec[16]` (4168), capacity from `sysconf(_SC_NPROCESSORS_ONLN)` (4178). `plant_smart_enter` (4223) partitions `size` into `chunk_size=256` chunks and emulates dispatch over the pool; queue pressure beyond `2×workers` expands the pool, beyond `g_vec_max` falls back to BALANCED (4239–4257).

### 5.3 Boundary handshake

`plant_boundary_block` (3720) enforces: FAST→SAFE blocked; SAFE→FAST/SMART/PERSISTENT blocked; PERSISTENT→SAFE blocked; SMART may call anything; BALANCED is unguarded. Blocked calls return `""` immediately — so cross-mode memory ownership never mixes.

---

## 6. EVAPORATE System — The Unified Model

### 6.1 How memory evaporates on scope exit

| Scope boundary | Mechanism |
|---|---|
| async task completes / is cancelled | arena segments refcount → cache or `free` (§4.1) |
| FAST action exits (next entry) | bump pointer rewinds; escaped mallocs freed at `plant_fast_reset` (§4.2) |
| ARC refs reach 0 | finalizer + edge/payload/object freed (§4.3); leases defer evaporation; GC reclaims cycles |
| SAFE action exits | channel buffer freed, worker released (§5.1) |
| process exits | `plant_async_drain` to idle, then OS reclamation (§4.5) |

### 6.2 Deterministic cleanup guarantees

- Every async task's arena is freed **exactly once** (teardown walks the tree; `plant_teardown_tree`, 3048).
- Every ARC object is destroyed **at most once** (removed from `g_arc_head` on destroy; `release`/`unlink` no-op on dead refs).
- FAST allocation never fails silently — it escalates to `malloc` with a one-time audit warning.
- Allocation failure in BALANCED (`plant_alloc`) aborts with a diagnostic — no silent NULLs.

### 6.3 No garbage-collector overhead

In BALANCED and FAST there is no GC at all. The only collector in the system is PERSISTENT's `plant_arc_gc`, whose mark pass is linear in live objects and runs every 1000 allocations (~sub-millisecond candidate scan, comment at 4344–4356).

### 6.4 Comparison with traditional GC

| | Chloroplast EVAPORATE | Rust / C++ RAII | Java / Python GC |
|---|---|---|---|
| Reclamation trigger | scope exit / refcount zero / task teardown | scope exit (drop) | background collector |
| Overhead | zero (BALANCED/FAST), amortized (ARC) | zero at runtime | stop-the-world / tracing |
| Leak risk | cycles in PERSISTENT (handled by tri-color GC), un-reclaimed BALANCED strings | none with RAII discipline | low (collector) |
| Manual control | none in language surface | explicit `free`/destructors | `gc.collect()` |
| Determinism | high | high | low |

---

## 7. Current Limitations & Gaps

1. **No explicit free/delete statements.** The language cannot free a `tx_t` string, list, or map. BALANCED-mode programs that allocate in a long-lived loop (the compiler itself included) rely on process exit for reclamation. There is no `plant_list_free` for `PlantArray`s at all — ownership of list items is caller-defined and unmetered.
2. **No manual memory control for advanced users.** ARC retain/release/link/lease/finalizer are usable via FFI (`ffi_arc_*` in tests, e.g. `persistent_cycle.plant`), but there are no `ARC LINK`-style statements; users cannot pin arena segment sizes, warm the segment cache, or flush `plant_fast_reset` from the language.
3. **Limited profiling/debugging tools.** Status strings exist (`plant_fast_status`, `plant_persist_status`, `plant_safe_status`, `plant_smart_status`, arena `miss%` in metrics) and the audit ring records `ARC_ALLOC/ARC_FREE/ARC_FINALIZE/FAST_ESCALATE` events, but there is no heap snapshot, leak report, or per-variable tracking. No sanitizer integration in the build.
4. **Leak potential in long-running PERSISTENT mode.** Automatic GC runs every `PERSIST_GC_INTERVAL` allocations — a program that sets the interval very high (or that leases objects for long periods, `PERSIST_LEASE_MS`) holds memory that only cycle detection or lease expiry will release. Escalated FAST mallocs (max 256) persist until `plant_fast_reset`.
5. **SAFE is emulated, not isolated.** Workers are in-process structs; there is no real process isolation or shared-memory model, so memory faults in a "worker" can still take down the program.
6. **`longjmp` (WEATHER) does not unwind memory.** CALM handlers that longjmp past FAST/PERSISTENT scopes leave their mission heaps as-is until the next enter/GC.
7. **Compiler self-hosting leaks by design.** `make self` compiles ~10K-line C that allocates compile-time strings it never frees; acceptable because the compiler is short-lived.

---

## 8. Future Enhancements

1. **Distributed memory.** `DistributedHeap` and the consistent hash ring are explicitly deferred in the code (`plant_runtime.c:4355–4356`). The PERSISTENT ARC heap is the natural substrate: per-node segments, hash-ring placement, and ARC leases as the eviction policy.
2. **Language-level memory control.** Statements for `FREE x.`, `ARC LINK/UNLINK`, and `FAST RESET.` would close the manual-control gap (Section 7.2) without touching BALANCED's zero-overhead default.
3. **Memory pooling and reuse.** The segment cache (`g_seg_cache`, cap 64) proves the model; extend it to fixed-size `tx_t` string slabs and reuse in `_cat` chains, and make the FAST heap grow-shrink adaptive like arena segments.
4. **Profiling.** A `plant_mem_report()` map (live bytes by owner: arena/FAST/ARC/BALANCED) and audit-scan for `FAST_ESCALATE`/repeated `ARC_ALLOC` without matching frees.
5. **Real process isolation for SAFE**, with channel copies replacing the in-process emulation, plus true zero-copy transfers above the `plant_channel` threshold (3515).
6. **RAII-style scope guarantees for mission modes** — e.g. emitting `plant_fast_reset` at every exit (not just the next enter) so fast heaps evaporate strictly within their action.

---

## Verification Appendix

- `make self` — full v1→v2→v3→v4→v5 self-hosting chain **converged** (351499 bytes).
- `make test` — native 20, generics 7, closures 6, regression 111, **0 failures** (including `fast_escalation`, `fast_audit`, `fast_security`, `persistent_cycle`, `persistent_cache`, `persistent_finalization`, `persistent_boundary`, `persistent_permissions`, `safe_*` suites).
- Generated code inspected (`build/plantc_v5.c`, 10481 lines): implicit `tx_t x = "";` declarations, `_cat`/`_cat3`/`_cat4` chains, `_from_long`/`_from_digit` numeric wrappers, `plant_async_alloc_state` state structs, mission enter/exit emission.
- Key line references: arena §2662–2866, FAST §3601–3709, SAFE §3471–3892, SMART §4160–4298, PERSISTENT §4343–4652, audit ring §3415–3468, codegen mission binding `src/plantc/codegen_c.plant:3269–3312`, main epilogue `:5363`.