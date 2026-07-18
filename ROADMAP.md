# PlantLang Roadmap: v0.24.0 (The Data Structures Release)

## What v0.23.0 Delivered

The previous roadmap targeted ACTION/SPECIES/LIST/MAP and memory management. Here's what was completed in **v0.23.0 (The Complex Structures Release)**:

### ✅ Completed: ACTION Functions (LLVM Backend)
Full function support with recursion, multiple typed parameters (NUM/SCL/TX/FACT), IF/ELSE bodies with multiple GIVE statements, and void actions.
- `ACTION` definition with typed params
- `REAP` calls with argument coercion
- `GIVE` return with type conversion through `i64` register
- Verified: factorial recursion, string returns, SCL params

### ✅ Completed: Rooted Depth System (Memory Architecture)
Replaced ad-hoc `alloca` with deterministic arena-based memory management:
- 64 depth levels with 64KB per-arena slabs
- Bump-pointer allocation — no GC, no fragmentation
- Four automatic cleanup mechanisms (Natural Exit, Forced Exit, Iteration Breath, Error Unwinding)
- Contract Law validation (CREATE destination depth ≤ current depth)

### ✅ Completed: WEATHER/SHELTER Exception Handling
Deterministic error handling with division-by-zero detection:
- Inline `fcmp oeq` checks before every division
- Error globals (`@_weather_msg`, `@_weather_type`, `@_weather_flag`)
- Unwind Chain arena cleanup between error source and SHELTER handler
- Typed storm matching + ANY_STORM catch-all

---

## 🚀 v0.24.0 Objectives

### 1. LIST & MAP Collection Types
**The biggest remaining gap.** PlantLang's LLVM backend rejects these at compile time. v0.24.0 will bring dynamic collections to native code:

| Sub-goal | Approach |
|---|---|
| **LIST (dynamic array)** | LLVM `struct` with `{ i64* data, i64 length, i64 capacity }`; heap-allocated via `@malloc`/`@realloc`/`@free` |
| **MAP (hash table)** | Open-addressing hash table struct; FNV-1a hashing for TX keys; linked-list chaining for collision resolution |
| **LIST literal syntax** | `CREATE xs(LIST) TO 1, 2, 3.` → compile-time array initialization |
| **MAP literal syntax** | `CREATE cfg(MAP).` with `LINK` key assignment → runtime hash inserts |
| **LIST operations** | `PUT`, `TAKE`, `SORT`, `SHAKE`, `COUNT`, `FIRST`, `LAST`, `SUM`, `AVG`, `MIN`, `MAX` |
| **MAP operations** | `LINK`, `UNLINK`, key access (`map:"key"`), `COUNT` |
| **CYCLE ... IN list** | Iteration over dynamic arrays |

### 2. SPECIES / BLOOM (Object-Oriented)

| Sub-goal | Approach |
|---|---|
| **SPECIES declaration** | LLVM struct with field offsets computed at compile time; vtable for methods |
| **BLOOM instantiation** | Heap allocation via `@malloc`; species methods receive `SELF` pointer |
| **PARENT inheritance** | Struct prefixing (parent fields at base, child fields appended) |
| **SELF reference** | Hidden first parameter in method ACTIONs |

### 3. Runtime Library Development (`libplantlang.so` / `runtime.c`)

| Sub-goal | Approach |
|---|---|
| **Hash table** | Open-addressing with FNV-1a (`runtime.c`) |
| **Dynamic array** | Amortized doubling realloc (`runtime.c`) |
| **Sorting** | Quicksort / mergesort for LIST (`runtime.c`) |
| **String operations** | `strlen`, `strcpy`, `strcat`, `strcmp` (already declared in LLVM IR) |
| **Printf formatting** | Shared format-string dispatch for SHOW output |

### 4. Compiler Hardening

| Sub-goal | Approach |
|---|---|
| **Contract Law: cross-depth access** | Enable `checkDepthAccess()` for SET/INCREASE/DECREASE/SHOW — warn on reads from deeper depths |
| **Contract Law: contracting syntax** | Implement `\N var -> M = expr` for explicit depth promotion |
| **Error coverage** | `srem` (modulo) zero-divisor check for WEATHER blocks |
| **String null safety** | Guard `@malloc(0)` in edge cases |

---

## 🛠️ Engineering Milestones

| Milestone | Task | Priority | Est. Effort |
| :--- | :--- | :--- | :--- |
| **M1** | `runtime.c` — hash table + dynamic array implementations | Critical | 2 weeks |
| **M2** | LLVM IR struct definitions for LIST/MAP | High | 1 week |
| **M3** | LLVM codegen: LIST literal, PUT/TAKE/SORT, CYCLE IN | High | 3 weeks |
| **M4** | LLVM codegen: MAP literal, LINK, key access | High | 2 weeks |
| **M5** | SPECIES struct layout + BLOOM instantiation in LLVM IR | Medium | 3 weeks |
| **M6** | SPECIES method dispatch + SELF parameter | Medium | 2 weeks |
| **M7** | Integration test suite for LIST/MAP/SPECIES (parity asserts) | High | 1 week |
| **M8** | Benchmark suite: interpreter vs compiled for collection-heavy workloads | Medium | 1 week |

---

## 🎯 Success Criteria

- **Full Parity**: LIST, MAP, and SPECIES behave identically in interpreter and LLVM-compiled binary
- **No Memory Leaks**: Valgrind-clean on all collection operations (no leaks, no use-after-free)
- **Test Count**: LLVM backend test suite grows from 37 → **75+** covering all new constructs
- **Performance**: Compiled LIST operations (sort, filter, map) within 2× of equivalent C

---

## 📅 Target Timeline

| Phase | Focus | Target |
|---|---|---|
| v0.23.x | Bug fixes, Contract Law enforcement, String null safety | Q3 2026 |
| **v0.24.0** | **LIST/MAP collections + SPECIES OOP** | **Q4 2026** |
| v0.25.0 | TAP file I/O, HARVEST networking, Flow pipeline | Q1 2027 |
| v0.26.0 | PULSE/WHENEVER reactive, VERIFY/SUITE native | Q2 2027 |

---

*PlantLang v0.24.0: From deterministic memory to dynamic data. COLLECTIONS — LOOP — OBJECT.*
