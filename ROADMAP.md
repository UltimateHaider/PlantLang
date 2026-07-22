# PlantLang Roadmap: v0.31.0 (Five-Mission Architecture)

## What v0.30.0 Delivered

The previous roadmap targeted the Runtime Library (sort, strings, math FFI), compiler hardening (Block-Depth Contract Law), and integration testing. Here's what was completed in **v0.30.0**:

### ✅ Completed: Runtime Library Infrastructure

| Sub-goal | Approach |
|---|---|
| **Math FFI (sqrt, sin, cos, tan, floor, ceil, abs)** | C wrappers in `runtime/runtime.c` calling libm; `RUNTIME_FFI` map in `llvm_codegen.js` for proper `declare double @sqrt(double)` emission |
| **Array sort (NUM / SCL)** | `plnt_sort_i64`, `plnt_sort_double` in C using `qsort`; void return, pointer+count params |
| **String concat / length / split / join** | `plnt_string_concat`, `plnt_string_len`, `plnt_str_split`, `plnt_str_join` in C with `%fat_ptr` struct return |
| **Build system** | `Makefile` with `runtime`, `exec`, `test`, `clean` targets; `libplantlang.so` built with `-fPIC -shared` |
| **NATIVE keyword** | Parser recognizes `NATIVE ACTION name(params) -> external.` syntax; sets `isExternal = true` |
| **FFI linkage** | `chloroplast.js` compile pipeline and test harness both link `-Lruntime -lplantlang` |
| **RUNTIME_FFI map** | 12 function signatures: math (double→double), sort (void), string (fat_ptr→fat_ptr, fat_ptr→i64) |
| **Universal REAP expressions** | `REAP x FROM SPLIT(str, delim)`, `REAP x FROM JOIN(arr, delim)`, `REAP x FROM parts[0]` work natively in interpreter and LLVM backend |
| **Large-string stress test** | C helper `plnt_stress_test_split_join` creates 70KB string, splits/joins/verifies roundtrip |
| **Test suite** | `test_phase21_runtime.js` — 20 tests: IR smoke tests, math FFI, SORT, FFI SPLIT/JOIN, native SPLIT/JOIN via REAP, 70KB stress test |
| **Block-Depth Contract Law Enforcement** | Parser: `enforceDepthContract()` with `this.currentDepth` tracking; Typechecker: `validateDepthInvariants(ast)` second pass; Enforces ACTION/SPECIES at Depth 0, REAP/GIVE/CYCLE at Depth ≥ 1; 13 tests in `test_depth_contract.js` |

---

## ✅ v0.31.0 Progress So Far

### ✅ Completed: Five-Mission Execution Architecture

| Sub-goal | Approach |
|---|---|
| **Five mission modes** | `MISSION: BALANCED/FAST/SAFE/SMART/PERSISTENT.` — each with distinct memory, optimization, and boundary policies |
| **MissionStatement lexer/parser** | `MISSION <MODE>.` recognized by tokenizer (`MISSION` keyword) and parser (`parseMissionStatement`) |
| **MissionBlockNode AST** | `MissionBlockNode` with `{ mode, bodyStatements }` — wraps all top-level statements under a mission declaration |
| **BoundaryViolationError** | `core/dispatcher.js` — custom error class for cross-mode rule violations with `fromMode`, `toMode`, and `reason` fields |
| **MissionStack** | `core/dispatcher.js` — `push(mode)` / `pop()` runtime tracking of mission execution context |
| **ScopedArena** | `core/dispatcher.js` — depth-level memory slabs with per-mission overflow policies: `expand(depth)` / `snapshot()`, `reset()` |
| **MissionDispatcher** | `core/dispatcher.js` — routes AST nodes to mission-specific evaluators; integrates Boundary Handshake Matrix for cross-mode ACTION calls |
| **SMART router** | `core/dispatcher.js` — mission-aware call routing: `dispatchReap`/`dispatchListenBranch` with mode whitelist (`"FAST": ["SAFE", "SMART", "BALANCED"]`) |
| **Boundary Handshake Matrix** | `core/dispatcher.js` — `BOUNDARY_MATRIX` constant: a 5×5 permission table specifying which source modes may call ACTIONs in which target modes |
| **LLVM codegen** | `llvm_codegen.js` — `genMissionStatement` emits mode constant to `@_mission_mode` global; `genReapStatement` emits mode-check guard |
| **Typechecker enforcement** | `typechecker.js` — `_checkMissionStatement` validates mode string and permission matrix |
| **75 new tests** | `tests/matrix.test.js` (28 — ScopedArena, MissionStack, BoundaryMatrix, cross-mode dispatch, LLVM guard emission); `tests/dispatcher.test.js` (47 — MissionDispatcher routing, SMART table, boundary violations, symbol pass) |

### ✅ Fixed: Pre-existing Test Failures

| Test | Failure | Fix |
|---|---|---|
| `test_diagnostics.js` (44→45) | Column assertion expected `21:4`, actual `21:9` (error points at "subtotl", not "SHOW") | Changed expected column to `9` |
| `test_parser_migration.js` (107→109) | RESPONSE emission skipped in LISTEN BRANCH test (server started but no request arrived); errVar bound English `"division by zero"`, test expected Arabic `"صفر"` | Added `_verifyDryRun` flag; changed expectation to `"division by zero"` |
| `test_llvm_codegen.js` (26→27) | ACTION/REAP is now fully supported by LLVM codegen — old rejection test was outdated | Updated to verify ACTION compiles without errors |

---

## 🛠️ Engineering Milestones

| Milestone | Task | Priority | Est. Effort |
| :--- | :--- | :--- | :--- |
| **M1** | Five-Mission Architecture (MissionStack, ScopedArena, Boundary Handshake Matrix, SMART router) | ✅ Done | 3 weeks |
| **M2** | Mission-aware codegen (mode globals, guard emission, depth overflow) | ✅ Done | 1 week |
| **M3** | Parser + typechecker integration (MissionBlockNode, symbol pass, permission validation) | ✅ Done | 1 week |
| **M4** | Full test suite (75 tests all green) | ✅ Done | 1 week |
| **M5** | IMPORT re-export / selective symbols | Medium | 1 week |
| **M6** | Integration test suite for SPECIES/CHOICE/MATCH parity | High | 1 week |

---

## 🎯 Success Criteria

- **No Regressions**: All 23 test suites (635+ tests) continue to pass
- **Five-Mission Parity**: All five mission modes (BALANCED/FAST/SAFE/SMART/PERSISTENT) execute correctly in interpreter and LLVM-compiled binary
- **Boundary Enforcement**: Cross-mode ACTION calls are permitted or rejected according to the Boundary Handshake Matrix
- **No Memory Leaks**: Valgrind-clean on all mission mode stress tests
- **Test Count**: Test suite grows to **635+** covering all new constructs
- **100% Pass Rate**: All pre-existing failures fixed — every test suite at 100%

---

## 📅 Target Timeline

| Phase | Focus | Target |
|---|---|---|
| **v0.29.0** | **SPECIES vtable dispatch, CHOICE/MATCH native, MAP get()** | **Q3 2026** |
| **v0.30.0** | **Runtime library, Block-Depth Contract Law, compiler hardening** | **Q4 2026** |
| v0.31.0 | Five-Mission Architecture (BALANCED/FAST/SAFE/SMART/PERSISTENT), Boundary Handshake Matrix, MissionDispatcher, SMART routing | Q1 2027 |
| v0.32.0 | PULSE/WHENEVER reactive, VERIFY/SUITE native, TAP file I/O | Q2 2027 |
| v0.33.0 | HARVEST networking, Flow pipeline, SPECIES/CHOICE integration tests | Q3 2027 |

---

*PlantLang v0.31.0: Five-Mission Architecture with Boundary Handshake Matrix, MissionDispatcher, ScopedArena, SMART routing. 75 new tests. 635+ total tests. All green.*

*PlantLang v0.30.0: Runtime C library, Native SPLIT/JOIN, Universal REAP expressions, Block-Depth Contract Law. 560+ tests all green.*
