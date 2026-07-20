# PlantLang Roadmap: v0.29.0 (The Polymorphic Dispatch Release)

## What v0.29.0 Delivered

The previous roadmap targeted SPECIES vtable dispatch, CHOICE/MATCH codegen, and compiler hardening. Here's what was completed in **v0.29.0**:

### ✅ Completed: SPECIES Vtable & Dynamic Dispatch (Phase 19)

| Sub-goal | Approach |
|---|---|
| **Vtable struct layout** | `i8*` vtable pointer added as field 0 of every species LLVM struct |
| **Method slot allocation** | Parent fields inherited first, method slots computed across the full parent chain |
| **Vtable globals** | Per-species `@species.Name.vtable = constant [N x i8*]` with function pointers for each method |
| **Constructor init** | Both `genCreateSpecies` and `genCreateBloomed` store the vtable pointer after zeroing fields |
| **Indirect dispatch** | `genMethodCallStatement` loads vtable from field 0, indexes to method slot, calls through function pointer |
| **Uniform calling convention** | All species method functions use `i8*` receiver (bitcast to concrete type inside function body) |
| **`__self` receiver** | LLVM `genFnDef` registers both `self` and `__self` in scope for SET field access |
| **Dynamic dispatch on expressions** | `MethodCall` expression nodes (`obj:method()`) in `compileAstExpr` also dispatch through vtable |

### ✅ Completed: CHOICE / MATCH LLVM Codegen (Phase 20)

| Sub-goal | Approach |
|---|---|
| **CHOICE struct layout** | `{ i64 tag, i64 payload }` — tag is variant index, payload is i64-compatible value |
| **Variant construction** | `Option.None` → `insertvalue {i64,i64} zeroinitializer, i64 tag, 0` |
| **Payload-bearing variants** | `Option.Some(10)` → `insertvalue` with tag + compiled payload value |
| **MATCH switch chain** | Extract tag via `extractvalue`, compare against each variant index, branch to clause body |
| **Payload binding** | `extractvalue` extracts payload, stored in arena for clause body access |
| **MAP `get()` → Option** | `genMapHas` for existence check + `_emitMapGetValue` for value probe → returns `{ tag, payload }` |

### ✅ Completed: SPECIES LLVM Bug Fixes

| Bug | Fix |
|---|---|
| `_checkMethodCallStatement` used `targetVar.speciesName` (undefined for BLOOM instances) | Store `speciesName` in typechecker variable info; use it for method resolution |
| `genSet` lacked `__self` in LLVM scope | Register `__self` alongside `self` in `genFnDef` receiver setup |
| `SelfExpression`/`BloomExpression` missing from `compileAstExpr` | Added cases: SelfExpression returns receiver pointer, BloomExpression allocates + zeroes instance |
| `BloomStatement` silently skipped | Now emits clear error: use `CREATE x TO BLOOM SpeciesName.` instead |

---

## 🚀 v0.30.0 Objectives

### 1. Runtime Library (`libplantlang.so`)

| Sub-goal | Approach |
|---|---|
| **Sorting (NUM/TX arrays)** | Quicksort / mergesort implemented in C, callable via FFI |
| **String operations** | `split`, `join`, `reverse`, `pad` — C implementations |
| **Math library expansion** | `sin`, `cos`, `tan`, `floor`, `ceil`, `round` as FFI functions |
| **Build system** | `Makefile` for `libplantlang.so`, auto-detected by `chloroplast compile` |
| **Format string** | Shared `printf`-style format dispatch for SHOW |

### 2. Language & Compiler Hardening

| Sub-goal | Approach |
|---|---|
| **Contract Law: cross-depth access** | Enable `checkDepthAccess()` for SET/INCREASE/DECREASE/SHOW |
| **Contract Law: contracting syntax** | Implement `\N var -> M = expr` for explicit depth promotion |
| **IMPORT re-export / symbols** | Selective symbol import from modules |
| **Error coverage** | `srem` (modulo) zero-divisor check for WEATHER blocks |
| **String null safety** | Guard `@malloc(0)` in edge cases |

### 3. Integration Testing & Parity

| Sub-goal | Approach |
|---|---|
| **SPECIES integration tests** | Test vtable dispatch, method overriding, SELF mutation across inheritance |
| **CHOICE/MATCH parity tests** | Verify interpreter and LLVM-compiled output match exactly for all CHOICE features |
| **LIST SORT parity** | Compiled sort matches interpreter's SORT behavior |
| **Benchmark suite** | Interpreter vs compiled for OOP-heavy workloads |

---

## 🛠️ Engineering Milestones

| Milestone | Task | Priority | Est. Effort |
| :--- | :--- | :--- | :--- |
| **M1** | Runtime library C implementation (sort, strings, math) | High | 3 weeks |
| **M2** | Contract Law cross-depth access enforcement | Medium | 1 week |
| **M3** | IMPORT re-export / selective symbols | Medium | 1 week |
| **M4** | Integration test suite for SPECIES/CHOICE/MATCH parity | High | 1 week |
| **M5** | Benchmark suite: interpreter vs compiled | Medium | 1 week |

---

## 🎯 Success Criteria

- **No Regressions**: All 17 existing test suites (~724 assertions) continue to pass
- **Full Parity**: SPECIES, CHOICE/MATCH, and LIST operations behave identically in interpreter and LLVM-compiled binary
- **No Memory Leaks**: Valgrind-clean on all collection operations
- **Test Count**: Test suite grows from ~724 → **850+** covering all new constructs
- **Performance**: Compiled SPECIES/CHOICE operations within 2× of equivalent C

---

## 📅 Target Timeline

| Phase | Focus | Target |
|---|---|---|
| **v0.29.0** | **SPECIES vtable dispatch, CHOICE/MATCH native, MAP get()** | **Q3 2026** |
| v0.30.0 | Runtime library (sort, strings, math), compiler hardening | Q4 2026 |
| v0.31.0 | TAP file I/O, HARVEST networking, Flow pipeline | Q1 2027 |
| v0.32.0 | PULSE/WHENEVER reactive, VERIFY/SUITE native | Q2 2027 |

---

*PlantLang v0.29.0: SPECIES vtable dispatch, CHOICE/MATCH LLVM codegen, MAP get() → Option. All 17 test suites green.*
