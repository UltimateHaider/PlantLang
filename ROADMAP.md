# PlantLang Roadmap: v0.30.0 (The Runtime Library Release)

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

## ✅ v0.30.0 Progress So Far

### ✅ Completed: Runtime Library Infrastructure

| Sub-goal | Approach |
|---|---|
| **Math FFI (sqrt, sin, cos, tan, floor, ceil, abs)** | C wrappers in `runtime/runtime.c` calling libm; `RUNTIME_FFI` map in `llvm_codegen.js` for proper `declare double @sqrt(double)` emission |
| **Array sort (NUM / SCL)** | `plnt_sort_i64`, `plnt_sort_double` in C using `qsort`; void return, pointer+count params |
| **String concat** | `plnt_string_concat` in C with `%fat_ptr` struct return |
| **String length** | `plnt_string_len` in C returning `i64` |
| **Build system** | `Makefile` with `runtime`, `exec`, `test`, `clean` targets; `libplantlang.so` built with `-fPIC -shared` |
| **NATIVE keyword** | Parser recognizes `NATIVE ACTION name(params) -> external.` syntax; sets `isExternal = true` |
| **FFI linkage in `chloroplast.js`** | `compileFile()` passes `-Lruntime -lplantlang` to gcc linker |
| **FFI linkage in tests** | `runCompiledLLVM` in `test_llvm_codegen.js` and `compileAndRun` in `test_phase21_runtime.js` both link against runtime lib |
| **RUNTIME_FFI map** | 12 function signatures: math (double→double), sort (void), string (fat_ptr→fat_ptr, fat_ptr→i64) |
| **String split/join** | `plnt_str_split` / `plnt_str_join` in C using sret + decomposed params; two-pass implementations with malloc |
| **LIST SORT parity** | Compiled `SORT` on `[NUM]`, `[SCL]` arrays calls `plnt_sort_i64` / `plnt_sort_double` |
| **Universal REAP expressions** | `REAP x FROM SPLIT(str, delim)`, `REAP x FROM JOIN(arr, delim)`, `REAP x FROM parts[0]` work natively in interpreter and LLVM backend |
| **Large-string stress test** | C helper `plnt_stress_test_split_join` creates 70KB string, splits/joins/verifies roundtrip |
| **Test suite** | `test_phase21_runtime.js` — 20 tests: IR smoke tests, math FFI, SORT, FFI SPLIT/JOIN, native SPLIT/JOIN via REAP, 70KB stress test |

### 🚀 v0.30.0 Remaining Objectives

### 1. Runtime Library Expansion

| Sub-goal | Approach |
|---|---|
| **String operations** | `reverse`, `pad` — C implementations in runtime.c, std/string.plnt bindings |
| **Sort for TX string arrays** | `plnt_sort_tx` in C using `qsort` + fat_ptr comparison |
| **Format string** | Shared `printf`-style format dispatch for SHOW |
| **MAP `get()` return type** | Fix codegen to correctly return `Option<V>` (see v0.29.0 MAP `get()` note) |

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
| **SPECIES integration tests** | Test vtable dispatch, method overriding, SELF mutation across inheritance (LLVM compiled vs interpreted) |
| **CHOICE/MATCH parity tests** | Verify interpreter and LLVM-compiled output match exactly for all CHOICE features |
| **LIST SORT parity** | Compiled sort matches interpreter's SORT behavior |
| **SPLIT/JOIN parity tests** | Native SPLIT and JOIN via REAP behave identically in interpreter and LLVM binary |
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

- **No Regressions**: All 19 test suites (549+ tests) continue to pass
- **Full Parity**: SPECIES, CHOICE/MATCH, LIST, and string SPLIT/JOIN operations behave identically in interpreter and LLVM-compiled binary
- **No Memory Leaks**: Valgrind-clean on all collection operations
- **Test Count**: Test suite grows from ~724 → **550+** covering all new constructs
- **Performance**: Compiled SPLIT/JOIN within 2× of equivalent C

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
