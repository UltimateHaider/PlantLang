# Technical Debt Register — PlantLang Chloroplast

## TD-001: PlantArray never freed
- **Status**: CLOSED (v0.51.2a)
- **Introduced**: v0.0.1 (always existed)
- **Fixed**: v0.51.2a
- **Impact**: 48–168 bytes per PlantArray (capacity-dependent)
- **Root cause**: `plant_list_free()` did not exist in the runtime
- **Fix**: Added `plant_list_free()` and `plant_array_free()` in `runtime/c/plant_memory.c` with recursive freeing. Codegen now emits `LIST_FREE()` via `_handle_func_paren` rewrite.
- **Scope**: Runtime + codegen
- **Verification**: valgrind-clean on all list tests

## TD-002: PlantTensor never freed
- **Status**: CLOSED (v0.51.2a)
- **Introduced**: v0.51.1
- **Fixed**: v0.51.2a
- **Impact**: 88–128 bytes per tensor (dimensionality-dependent)
- **Root cause**: Generated code never called `plant_tensor_free()`
- **Fix**: Codegen now emits `TENSOR_FREE()` via `_handle_func_paren` rewrite. Declaration added to `plant_compat.h`.
- **Scope**: Codegen + compat header
- **Verification**: valgrind-clean on all tensor tests

## TD-003: to_string buffer never freed
- **Status**: CLOSED (v0.51.2a)
- **Introduced**: v0.51.1
- **Fixed**: v0.51.2a
- **Impact**: 1,024 bytes per SHOW TENSOR statement
- **Root cause**: `plant_tensor_to_string()` returns a heap-allocated string; generated code passed it directly to `plant_iReport_print()` without `free()`.
- **Fix**: `gen_show_stmt` now emits `{ char* __str = ...; plant_iReport_print(r, __str); free(__str); }` for tensor SHOW statements. Added `plant_tensor_to_string_static()` for buffer-based output.
- **Scope**: Codegen + runtime
- **Verification**: valgrind-clean on all tensor display tests

## TD-004: valgrind-check was measuring compiler, not runtime
- **Status**: FIXED (v0.51.1)
- **Introduced**: v0.51.0b
- **Fixed**: v0.51.1
- **Impact**: Compiler leaks (87KB+) masked runtime leaks (1–2KB)
- **Root cause**: Original `valgrind-check` ran `valgrind bin/Chloroplast tests/native/$$test.plant` — valgrinding the compiler process, not the compiled test binary.
- **Fix**: Target now compiles test → gcc → valgrind on compiled binary.
- **Scope**: Makefile only

## TD-005: Generic plant_free dispatcher — PARTIAL
- **Status**: PARTIAL (documentation was ahead of implementation)
- **Introduced**: v0.51.2a (planned), v0.51.2b (partially applied)
- **Target fix**: v0.51.2c or v0.52.0
- **Impact**: LOW — functional behavior is correct, but the dispatcher is not fully unified.
- **Actual state**: codegen still emits type-specific calls:
  - `LIST_FREE(x)`   → `plant_list_free(x)`
  - `TENSOR_FREE(x)` → `plant_tensor_free(x)`
  - `FREE(x)`        → `plant_free(x)`
- **Planned state (v0.51.2c+)**: all three should route through `plant_free(x)` for uniformity.
- **Note**: `plant_free` already exists and dispatches by magic. The change is only in codegen emission.
- **Related**: TD-006 (double-free) depends on how `plant_free` interacts with auto-cleanup.

## TD-006: Double-free in dispatcher_backcompat
- **Status**: OPEN
- **Introduced**: v0.51.2b (test added in v0.51.2a-b transition)
- **Target fix**: v0.51.2c or v0.52.0
- **Impact**: MEDIUM — 1 failing native test (`dispatcher_backcompat`).
- **Symptom**: Runtime aborts with "double free detected in tcache 2" (exit 134).
- **Root cause**: codegen emits both `plant_list_free(L3)` AND `L3 = plant_mem_free((tx_t)L3)` for the same variable. The second free is a use-after-free.
- **Unsafe proposed fix (REJECTED)**: zeroing magic before `free()` and checking `magic==0` in `plant_mem_free`. This reads from freed memory (undefined behavior).
- **Correct fix direction**: codegen must track that a variable was explicitly freed via `*_FREE`, and skip auto `plant_mem_free` for it. Requires variable-lifecycle tracking in codegen.
- **Related**: Type System Audit planned for v0.52.0.

## TD-007: SHOW COUNT(L) produces invalid C
- **Status**: OPEN
- **Introduced**: pre-existing (discovered in v0.51.2b testing)
- **Target fix**: v0.51.2c
- **Impact**: LOW-MEDIUM — `COUNT(L)` works inline but not in `SHOW`.
- **Symptom**: `SHOW COUNT(L).` produces a GCC error.
- **Generated C (wrong)**: `plant_array_length()( L )` — invalid C
- **Expected C**: `plant_array_length( L )`
- **Root cause**: `_handle_func` splits by `"COUNT "` (with trailing space) and doesn't match `"COUNT("`. The `COUNT` prefix is partially translated but not merged with the argument.
- **Correct fix direction**: handle `"COUNT("` in the same dispatch as `"COUNT "`.
- **Related**: Type System Audit planned for v0.52.0.

## TD-008: TENSOR([...]) input list not freed
- **Status:** OPEN
- **Introduced:** v0.51.1 (TENSOR type introduction)
- **Target fix:** v0.51.2d or v0.52.0
- **Impact:** LOW-MEDIUM — every `TENSOR([...])` literal leaks the input list.
- **Root cause:** `plant_tensor_from_list(list)` in `runtime/c/plant_tensor.c` does not free or take ownership of the input list.
- **Affected tests:** tensor_basic, tensor_display, list_free, dispatcher_full, freed_vars_mixed, dispatcher_backcompat (all pre-existing, all outside v0.51.2c.1 scope).
- **Impact estimate:** ~40-72 bytes per `TENSOR([...])` literal.
- **Fix direction (one of):**
  (a) `plant_tensor_from_list` takes ownership and frees the input list, OR
  (b) codegen emits `plant_list_free` after `plant_tensor_from_list`.
- **Constraint:** requires runtime change → out of scope for v0.51.2c.1.
- **Discovered by:** valgrind on freed_vars_mixed and dispatcher_backcompat (v0.51.2c.1).

## TD-009: Structs are fundamentally broken
- **Status:** OPEN
- **Introduced:** pre-v0.51.1 (long-standing limitation)
- **Target fix:** v0.52.0 (Type System Audit)
- **Impact:** HIGH — structs cannot be used reliably.
- **Discovered by:** DeepSeek V4 Flash during v0.51.2c.2 Phase 2.

### Symptoms
1. `STRUCT Holder { values: LIST }` + `CREATE h TO Holder(...)`:
   - Emits `Holder(...)` (undefined function) → linker error.
   - Structs are represented as `tx_t` map handles, not stack structs.
2. `LIST_FREE(h.values)`:
   - Emits `plant_list_free(h . values)` — invalid C.
   - Dot-access is not lowered inside free-call argument position.
3. `SPECIES Holder: values: LIST. /SPECIES.`:
   - Compiler SIGSEGV (exit 139, no output file).
4. Map literals `{a: [1, 2]}`:
   - Keys need quotes (`{"a": ...}`).
   - Unquoted keys → undeclared variable.
5. Field reads (`SHOW m.a`):
   - ✅ Work — lowered to `_map_get(m, "a")`.
   - So reads work but frees don't.

### Root Cause
- Structs in PlantLang are tx_t (map handle) values, not C structs.
- Dot-access is lowered only in read positions, not in free positions.
- `CREATE x TO TypeName(...)` is not recognized as a struct constructor.

### Out of Scope for v0.51.2c.2
- Struct field tracking
- Any struct lifecycle management
- Map literal syntax

### Recommended Fix Path
- v0.52.0 (Type System Audit):
  - Decide struct representation (stack C struct? map handle?).
  - Unify dot-access lowering across read/free positions.
  - Support `CREATE x TO StructType(...)`.
  - Fix SPECIES parser crash.
  - Support map literals.

### User Guidance
- Do NOT rely on structs in production code.
- Use explicit `_map_get` / FFI where possible.
- Free struct fields manually (if supported at all).

## TD-010: plant_list_free does not free multi-digit _from_long string items
- **Status:** OPEN
- **Introduced:** v0.51.2a (LIST_FREE / plant_list_free introduction)
- **Target fix:** v0.51.2d or v0.52.0
- **Impact:** LOW — small leak (~3 bytes per multi-digit integer inside a freed list).
- **Root cause:** `_from_long()` strdup's a string for multi-digit numbers (single digits use a static table); `plant_list_free()` frees nested containers but skips primitive string items (`/* else: primitive — not freed here */`).
- **Affected tests:** freed_vars_nested_func (6 B), freed_vars_comprehensive (6 B); any `LIST_FREE` on a list containing integers `>= 10`.
- **Evidence:** isolated `CREATE L TO [1,2]. LIST_FREE(L).` → 0 B; `[10,20]` → 6 B; `[10,20,30,40]` → 12 B.
- **Constraint:** requires runtime change (`plant_memory.c` / `plant_compat.h`) → out of scope for v0.51.2c.2.
- **Discovered by:** valgrind on freed_vars_nested_func (v0.51.2c.2).
