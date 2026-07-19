# PlantLang Roadmap: v0.27.0 (The OOP & Systems Release)

## What v0.26.0 Delivered

The previous roadmap targeted MAP hash tables with native LLVM compilation for Phase 14. Here's what was completed in **v0.26.0 (The Collections Release)**:

### ✅ Completed: MAP Type (Hash Table) — Full LLVM Codegen

Open-addressing HashMap with linear probing, fully compiled:

| Sub-goal | Status |
|---|---|
| **MAP syntax** | `CREATE m(MAP[NUM,TX]).` with typed key/value parameters; `{ key: value }` dict literals |
| **LINK statement** | `LINK key WITH value IN map.` — compiled via `genMapPut` |
| **Bucket layout** | `{ i1 is_occupied, key_type, value_type }` — arena-allocated arrays |
| **Hash function** | djb2 for TX keys (inline LLVM IR), identity hash for NUM keys |
| **Linear probing** | Full probe loop with match/empty/collision three-way branching |
| **Automatic growth** | Load factor > 0.75 triggers 2× capacity doubling with full rehash |
| **`m.has(key)`** | Compiled natively — returns FACT (true/false) |
| **`m.get(key)`** | Interpreter-only (returns `Option<V>`, needs MATCH codegen) |
| **`m.put(key, value)`** | Compiled via `LinkStatement` → `genMapPut` |
| **Backward compat** | Legacy untyped `MAP` (plain object) still works in interpreter |
| **Type checker** | Full validation: key/value arity, type matching, MAP[K,V] inference |

### ✅ Completed: LLVM Codegen Robustness Fixes

- `llvmType()` — MAP types return `%fat_ptr` (was `null`, causing null-pointer stores)
- `@llvm.memset.p0i8.i64` — conditional declaration for bucket array zeroing
- Grow loop rehash branch — fixed no-op loop (both targets pointed to exit)
- `storeL`/`skipL` separation — fixed unterminated basic block in grow rehash
- `genMapHas` returns `FACT` — SHOW now prints `true`/`false` matching interpreter
- `mapBucketSize` padding — bucket stride now 40 bytes (was 33), fixing multi-element corruption
- `genShow` handles `MethodCall` — enables `SHOW m.has(1).` in compiled mode

### ✅ Completed: Test Expansion
- Phase 14 (MAP): 17 tests
- LLVM backend: 50 tests (was 46)
- **13 test files total, ~634 assertions, all green**

---

## 🚀 v0.27.0 Objectives

### 1. SPECIES / BLOOM (Object-Oriented) in LLVM Backend

| Sub-goal | Approach |
|---|---|
| **SPECIES declaration** | LLVM struct with field offsets computed at compile time; vtable for methods |
| **BLOOM instantiation** | Heap allocation via arena; species methods receive `SELF` pointer |
| **PARENT inheritance** | Struct prefixing (parent fields at base, child fields appended) |
| **SELF reference** | Hidden first parameter in method ACTIONs |
| **Method dispatch** | Colon syntax `obj:method` — compiled to function call with SELF pointer |

### 2. LIST Operations in LLVM Backend

| Sub-goal | Approach |
|---|---|
| **LIST literal** | `CREATE xs(LIST) TO 1, 2, 3.` → compile-time array initialization |
| **LIST operations** | `SORT`, `SHAKE`, `COUNT`, `FIRST`, `LAST`, `SUM`, `AVG`, `MIN`, `MAX` |
| **CYCLE ... IN list** | Iteration over dynamic arrays |
| **Index access** | `xs[0]`, `SET xs[0] TO val` |

### 3. CHOICE / MATCH in LLVM Backend

| Sub-goal | Approach |
|---|---|
| **CHOICE struct layout** | Tag + payload union — `{ i64 tag, { i64, %fat_ptr } payload }` |
| **MATCH codegen** | Switch on tag, branch to clause body with payload binding |
| **`m.get(key)`** | Enable in compiled mode by compiling the returned `Option<V>` |

### 4. Runtime Library (`runtime.c` → `libplantlang.so`)

| Sub-goal | Approach |
|---|---|
| **Hash table** | Open-addressing with FNV-1a (`runtime.c`) |
| **Dynamic array (mature)** | Stabilize realloc/shrink patterns |
| **Sorting** | Quicksort / mergesort for LIST (`runtime.c`) |
| **Std library expansion** | Add `math`, `lists`, `maps` standard modules |
| **Printf formatting** | Shared format-string dispatch for SHOW output |

### 5. Compiler & Language Hardening

| Sub-goal | Approach |
|---|---|
| **Contract Law: cross-depth access** | Enable `checkDepthAccess()` for SET/INCREASE/DECREASE/SHOW |
| **Contract Law: contracting syntax** | Implement `\N var -> M = expr` for explicit depth promotion |
| **Error coverage** | `srem` (modulo) zero-divisor check for WEATHER blocks |
| **String null safety** | Guard `@malloc(0)` in edge cases |
| **IMPORT re-export / symbols** | Selective symbol import from modules |

---

## 🛠️ Engineering Milestones

| Milestone | Task | Priority | Est. Effort |
| :--- | :--- | :--- | :--- |
| **M1** | SPECIES struct layout + BLOOM instantiation in LLVM IR | High | 3 weeks |
| **M2** | SPECIES method dispatch + SELF parameter | High | 2 weeks |
| **M3** | CHOICE/MATCH codegen for LLVM backend | High | 2 weeks |
| **M4** | LLVM codegen: LIST literal, SORT, CYCLE IN | Medium | 2 weeks |
| **M5** | Std library expansion (math, lists, maps modules) | Medium | 1 week |
| **M6** | Integration test suite for SPECIES/LIST/CHOICE parity | High | 1 week |
| **M7** | Benchmark suite: interpreter vs compiled for OOP-heavy workloads | Medium | 1 week |

---

## 🎯 Success Criteria

- **Full Parity**: SPECIES, LIST, and CHOICE/MATCH operations behave identically in interpreter and LLVM-compiled binary
- **No Memory Leaks**: Valgrind-clean on all collection operations (no leaks, no use-after-free)
- **Test Count**: Test suite grows from ~634 → **850+** covering all new constructs
- **Performance**: Compiled SPECIES/LIST operations within 2× of equivalent C

---

## 📅 Target Timeline

| Phase | Focus | Target |
|---|---|---|
| **v0.27.0** | **SPECIES OOP + CHOICE/MATCH native + LIST native** | **Q2 2027** |
| v0.28.0 | TAP file I/O, HARVEST networking, Flow pipeline | Q3 2027 |
| v0.29.0 | PULSE/WHENEVER reactive, VERIFY/SUITE native | Q4 2027 |

---

*PlantLang v0.26.0: MAP hash tables. Native LLVM codegen. 634 tests all green.*
