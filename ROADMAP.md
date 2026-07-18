# PlantLang Roadmap: v0.25.0 (The Collections Release)

## What v0.24.0 Delivered

The previous roadmap targeted IMPORT/FFI and the Standard Library foundation. Here's what was completed in **v0.24.0 (The Modularity Release)**:

### ✅ Completed: IMPORT Statement & Module System
Multi-file PlantLang programs with cycle detection and AST merging:
- `IMPORT "path".` syntax for loading external `.plnt` files
- Relative, absolute, and `std/`-prefixed path resolution
- Cycle detection with clear error messages
- AST merging — imported statements spliced into importing program
- `ImportStatementNode` with `path`, `statements`, `resolvedAbsPath`

### ✅ Completed: FFI (Foreign Function Interface)
Call native C functions directly from PlantLang:
- `ACTION name(params) -> external.` FFI declaration syntax
- LLVM backend emits `declare` IR with proper type signatures
- FFI stubs pre-registered in interpreter for all `runtime_bridge` functions
- Type checker validates FFI signatures against known bridge functions

### ✅ Completed: Standard Library Foundation (`std/`)
Three core modules providing essential functionality:
- `std/io.plnt` — `print`, `println`, `plant_printf`, `plant_puts`
- `std/string.plnt` — `len`, `upper`, `lower`, `trim`, `contains`, `split`, `replace`, `concat`
- `std/prelude.plnt` — auto-injected core definitions (TRUE, FALSE, _BOOT)
- Auto-prelude injection: every program implicitly imports `std/prelude.plnt`

### ✅ Completed: Core Runtime Bridge (`core/runtime_bridge.c`)
C bridge implementing 10 FFI targets for I/O and string operations, linked into compiled binaries via LLVM `declare` + extern resolution.

### ✅ Completed: Test Expansion
- Phase 7 test suite: 30 test groups for Module System & FFI
- Phase 8 test suite: 8 integration groups for Standard Library
- LLVM backend expanded from 37 → 46 smoke tests
- 7 test files total, ~300+ total assertions, all green

---

## 🚀 v0.25.0 Objectives

### 1. LIST & MAP Collection Types
**The biggest remaining gap.** PlantLang's LLVM backend rejects these at compile time. v0.25.0 will bring dynamic collections to native code:

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

### 3. Runtime Library Expansion (`runtime.c` → `libplantlang.so`)

| Sub-goal | Approach |
|---|---|
| **Hash table** | Open-addressing with FNV-1a (`runtime.c`) |
| **Dynamic array** | Amortized doubling realloc (`runtime.c`) |
| **Sorting** | Quicksort / mergesort for LIST (`runtime.c`) |
| **Std library expansion** | Add `math`, `lists`, `maps` standard modules |
| **Printf formatting** | Shared format-string dispatch for SHOW output |

### 4. Compiler & Language Hardening

| Sub-goal | Approach |
|---|---|
| **Contract Law: cross-depth access** | Enable `checkDepthAccess()` for SET/INCREASE/DECREASE/SHOW — warn on reads from deeper depths |
| **Contract Law: contracting syntax** | Implement `\N var -> M = expr` for explicit depth promotion |
| **Error coverage** | `srem` (modulo) zero-divisor check for WEATHER blocks |
| **String null safety** | Guard `@malloc(0)` in edge cases |
| **IMPORT re-export / symbols** | Selective symbol import from modules |

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
| **M7** | Std library expansion (math, lists, maps modules) | High | 1 week |
| **M8** | Integration test suite for LIST/MAP/SPECIES (parity asserts) | High | 1 week |
| **M9** | Benchmark suite: interpreter vs compiled for collection-heavy workloads | Medium | 1 week |

---

## 🎯 Success Criteria

- **Full Parity**: LIST, MAP, and SPECIES behave identically in interpreter and LLVM-compiled binary
- **No Memory Leaks**: Valgrind-clean on all collection operations (no leaks, no use-after-free)
- **Test Count**: LLVM backend test suite grows from 46 → **85+** covering all new constructs
- **Performance**: Compiled LIST operations (sort, filter, map) within 2× of equivalent C

---

## 📅 Target Timeline

| Phase | Focus | Target |
|---|---|---|
| v0.24.x | Bug fixes, Contract Law enforcement, String null safety | Q3 2026 |
| **v0.25.0** | **LIST/MAP collections + SPECIES OOP** | **Q4 2026** |
| v0.26.0 | TAP file I/O, HARVEST networking, Flow pipeline | Q1 2027 |
| v0.27.0 | PULSE/WHENEVER reactive, VERIFY/SUITE native | Q2 2027 |

---

*PlantLang v0.25.0: From modular programs to dynamic data. COLLECTIONS — OBJECTS — NATIVE.*

