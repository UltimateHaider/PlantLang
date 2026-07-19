# PlantLang Roadmap: v0.26.0 (The Systems Release)

## What v0.25.0 Delivered

The previous roadmap targeted SHAPE structs, Methods, Dynamic Arrays, and CHOICE/MATCH pattern matching. Here's what was completed in **v0.25.0 (The Collections Release)**:

### ✅ Completed: SHAPE (Struct Types)
User-defined aggregate types with named, typed fields:
- `SHAPE Point { x(NUM), y(NUM) }.` declaration syntax
- `CREATE p(Point) TO Point{ 10, 20 }.` instantiation
- Field access (`.x`) and mutation (`SET p.x TO 99.`)
- Zero-field structs and struct-of-struct composition
- Full type-checker and LLVM codegen support

### ✅ Completed: Methods on Structs
ACTIONs with typed `SELF` receiver:
- `ACTION (self(Point)) move(x(NUM), y(NUM)), ... /ACTION.` declaration
- `REAP _ FROM p:move, 5, 10.` colon-dispatch call syntax
- Type checker validates receiver type on method calls

### ✅ Completed: Dynamic Arrays
Runtime array growth with push/pop:
- `PUT val INTO xs.` — amortized-O(1) append with capacity doubling
- `TAKE val FROM xs.` — pop from end with shrink
- Empty list declaration: `CREATE xs(LIST) TO.`

### ✅ Completed: Tagged Unions (CHOICE) & Pattern Matching (MATCH)
Type-safe sum types with exhaustive case analysis:
- `CHOICE Option { Some(NUM), None }.` — sum type with payload variants
- `Option.Some(10)`, `Option.None` — variant construction via member-access syntax
- `MATCH opt { Some(v) -> { SHOW v } None -> { SHOW 0 } }.` — exhaustive match with payload binding
- Variant names accepted as both IDENT and KEYWORD tokens after `.`
- Non-exhaustive matches produce compile-time errors

### ✅ Completed: Test Expansion
- Phase 9 (Structs): 70 tests
- Phase 10 (Arrays): 58 tests
- Phase 11 (Methods): 47 tests
- Phase 12 (Array Growth): 64 tests
- Phase 13 (CHOICE/MATCH): 64 tests
- **12 test files total, ~613 assertions, all green**

---

## 🚀 v0.26.0 Objectives

### 1. MAP Type (Hash Table)
**The last major collection type missing from native compilation.** MAPs enable key-value storage for config, HTTP headers, and structured data:

| Sub-goal | Approach |
|---|---|
| **MAP struct** | LLVM `struct` with `{ i8** keys, i8** vals, i64 length, i64 capacity }`; heap-allocated |
| **MAP literals** | `CREATE cfg(MAP).` with `LINK` key assignment or inline init syntax |
| **Key access** | `map:"key"` — string-keyed lookup compiled to hash + probe |
| **MAP operations** | `LINK`, `SET map:"k" TO v`, `COUNT`, key existence check |
| **MAP iteration** | `CYCLE key IN keys(map)` → sequential scan over live entries |

### 2. SPECIES / BLOOM (Object-Oriented) in LLVM Backend

| Sub-goal | Approach |
|---|---|
| **SPECIES declaration** | LLVM struct with field offsets computed at compile time; vtable for methods |
| **BLOOM instantiation** | Heap allocation via `@malloc`; species methods receive `SELF` pointer |
| **PARENT inheritance** | Struct prefixing (parent fields at base, child fields appended) |
| **SELF reference** | Hidden first parameter in method ACTIONs |

### 3. LIST Operations in LLVM Backend

| Sub-goal | Approach |
|---|---|
| **LIST literal** | `CREATE xs(LIST) TO 1, 2, 3.` → compile-time array initialization |
| **LIST operations** | `SORT`, `SHAKE`, `COUNT`, `FIRST`, `LAST`, `SUM`, `AVG`, `MIN`, `MAX` |
| **CYCLE ... IN list** | Iteration over dynamic arrays |
| **Index access** | `xs[0]`, `SET xs[0] TO val` |

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
| **M1** | `runtime.c` — hash table implementation | Critical | 2 weeks |
| **M2** | LLVM IR struct definitions for MAP | High | 1 week |
| **M3** | LLVM codegen: MAP literal, LINK, key access | High | 2 weeks |
| **M4** | LLVM codegen: LIST literal, SORT, CYCLE IN | High | 2 weeks |
| **M5** | SPECIES struct layout + BLOOM instantiation in LLVM IR | Medium | 3 weeks |
| **M6** | SPECIES method dispatch + SELF parameter | Medium | 2 weeks |
| **M7** | Std library expansion (math, lists, maps modules) | High | 1 week |
| **M8** | Integration test suite for MAP/SPECIES/LIST parity | High | 1 week |
| **M9** | Benchmark suite: interpreter vs compiled for collection-heavy workloads | Medium | 1 week |

---

## 🎯 Success Criteria

- **Full Parity**: MAP, SPECIES, and LIST operations behave identically in interpreter and LLVM-compiled binary
- **No Memory Leaks**: Valgrind-clean on all collection operations (no leaks, no use-after-free)
- **Test Count**: Test suite grows from ~613 → **850+** covering all new constructs
- **Performance**: Compiled MAP/LIST operations within 2× of equivalent C

---

## 📅 Target Timeline

| Phase | Focus | Target |
|---|---|---|
| **v0.26.0** | **MAP collections + SPECIES OOP + LIST native** | **Q1 2027** |
| v0.27.0 | TAP file I/O, HARVEST networking, Flow pipeline | Q2 2027 |
| v0.28.0 | PULSE/WHENEVER reactive, VERIFY/SUITE native | Q3 2027 |

---

*PlantLang v0.26.0: From tagged unions to systems programming. MAPS — OBJECTS — NATIVE COLLECTIONS.*

