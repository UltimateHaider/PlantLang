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

## TD-005: Generic plant_free dispatcher — CLOSED

- **Status:** ✅ CLOSED in v0.51.2c.3
- **Closed by:** unified dispatcher — LIST_FREE, TENSOR_FREE, and FREE
  all emit `plant_free((void*)x)`.
- **Historical:** v0.51.2a-b emitted type-specific calls
  (`plant_list_free`, `plant_tensor_free`). These remain in the
  runtime (public API) but are no longer emitted.
- **Related:** TD-006 (double-free, closed in c.1).

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

## TD-007: COUNT in SHOW/IF/CYCLE — CLOSED

- **Status:** ✅ CLOSED in v0.51.2c.3
- **Fixed:** COUNT works in all tested contexts:
  - `SHOW COUNT(L).`
  - `IF COUNT(L) > 0,`
  - `CYCLE i FROM 1 TO COUNT(L),`
  - `SHOW COUNT (L).` [space form]
  - `SHOW COUNT L.` [bare form]
- **Fix:** `translate_expr` now applies `_handle_func_paren` for COUNT
  before `_handle_func`. Tokens joined with spaces → `"COUNT ( L )"`
  is now correctly lowered to `plant_array_length(L)`.
- **Related:** TD-011 (complex expressions).

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

## TD-011: COUNT in complex expressions — MOSTLY FIXED

- **Status:** MOSTLY FIXED in v0.51.2c.3 (edge cases remain)
- **Target for remaining edges:** v0.51.2d
- **Impact:** LOW — common cases work.

### Fixed in v0.51.2c.3
- `SHOW COUNT(L).`
- `IF COUNT(L) > 0, ...`
- `CYCLE i FROM 1 TO COUNT(L), ...`
- `COUNT(L)` in mixed expressions
- **`COUNT(L1) + COUNT(L2)` (complex case now works!)**

### Possibly unfixed (v0.51.2d investigation)
- `COUNT(FUNC(x))` where FUNC is a runtime call
- `COUNT(L)` inside nested function calls
- Deeply nested COUNT expressions
- COUNT combined with other meta-operations

### User Guidance
If a complex COUNT expression fails, refactor with a temp:
```plantlang
CREATE n TO COUNT(L).
SHOW n + COUNT(L2).
```

- **Related:** TD-007 (original COUNT fix), TD-012 (SUITE nums gap).

## TD-012: collect_nums_walk does not recurse into suite_stmt

- **Status:** OPEN
- **Introduced:** pre-v0.51.2 (long-standing)
- **Target fix:** v0.51.2d
- **Impact:** LOW-MEDIUM — a bare `SHOW` of a CYCLE iterVar inside a
  SUITE body is not `_from_long`-wrapped.
- **Root cause:** `collect_nums_walk` does not scan `suite_stmt` bodies.
  Numeric variables declared inside a SUITE are not added to `nums`,
  so `expr_is_numeric` returns 0 for them → no `_from_long` wrapping.
- **Workaround:** use top-level `ACTION main` form.
- **Discovered by:** v0.51.2c.3 test authoring.
- **Related:** TD-006 (lifecycle tracking), TD-011 (COUNT in contexts).

## TD-013: valgrind-check-tensor masks valgrind exit code
- **Status:** OPEN
- **Introduced:** v0.51.0b or earlier
- **Target fix:** v0.51.2d
- **Impact:** MEDIUM — valgrind-check-tensor may report PASS even when
  valgrind finds leaks, because `valgrind ... | tee` masks the exit
  code (the pipeline's exit status is tee's, not valgrind's).
- **Fix direction:** use `set -o pipefail` (POSIX) or capture exit
  code before piping, or use process substitution.
- **Discovered by:** v0.51.2c.3 documentation review.
