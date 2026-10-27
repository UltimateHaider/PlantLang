# Technical Debt Register — PlantLang Chloroplast

> **Status (v0.60.0d): 0 known bugs open** — 0 silent-wrong, 0 crash,
> 0 leak. TD-048 is CLOSED (higher-order AD: 1st/2nd order complete;
> 3rd+ unsupported by design with an explicit error gate). TD-055 is a
> feature; TD-016/TD-017 remain DEFERRED.

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

## TD-006: Double-free in dispatcher_backcompat — CLOSED
- **Status**: ✅ CLOSED in v0.51.2c.1 (variable-lifecycle tracking).
- **Introduced**: v0.51.2b (test added in v0.51.2a-b transition)
- **Symptom**: Runtime aborted with "double free detected in tcache 2".
- **Root cause**: codegen emitted both `plant_list_free(L3)` AND
  `L3 = plant_mem_free((tx_t)L3)` for the same variable.
- **Fix**: `freed_vars` (env slot 13) tracks explicitly-freed variables;
  auto-cleanup skips them.
- **Residual**: frees inside a nested block are block-scoped (invisible
  to the enclosing scope) — a cross-scope double-free remains. See **TD-015**.
- **Related**: TD-015 (global/cross-scope tracking, v0.52.0).

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

## TD-008: TENSOR([...]) input list not freed — CLOSED
- **Status:** ✅ CLOSED in v0.51.2d.1
- **Fixed:** codegen now frees the input list after
  `plant_tensor_from_list`, at both CREATE and SHOW sites.
- **Safety guard:** free only when the argument is a fresh
  `plant_list_make(...)` literal, never a user variable.
- **Residual:** `tensor_basic` still leaks 128 B from its own
  unfreed `T4`/`T5` tensors (test hygiene — see TD-014).

## TD-009: Structs are fundamentally broken — CLOSED
- **Status:** ✅ CLOSED in v0.52.4
- **Introduced:** pre-v0.51.1 (long-standing limitation)
- **Impact:** HIGH — structs cannot be used reliably.
- **Fixed (v0.52.3e):**
  - `STR X` CREATE/LET vars → `tx_t` (map handle, matching SPECIES).
  - Named brace literal `X{k:v}` → `plant_map_set` chain (bare keys quoted).
  - `p.x` → `_map_get` (already worked for handles).
  - Scoped via `type_info` so registry scalars (BYT/UBT/SHR/USH/ULO) keep their C types.
- **Fixed (v0.52.3f):**
  - Positional `X(v1, v2, …)` construction → `plant_map_set` chain,
    using the struct template field order (threaded via env slot 14).
- **Fixed (v0.52.4):**
  - Embedded struct methods `STR Type { ACTION m() {…} }` lifted to
    `tx_t m(tx_t self, …)`.
  - Dotted-decl `ACTION Type.method()` rewritten to a bare method name
    with a `self` (TXT) receiver param.
- **Remaining:** none.
- **Related:** TD-036.

### Symptoms
1. `STR Holder { values: LIST }` + `CREATE h TO Holder(...)`:
   - Emits `Holder(...)` (undefined function) → linker error.
   - Structs are represented as `tx_t` map handles, not stack structs.
2. `LIST_FREE(h.values)`:
   - Emits `plant_list_free(h . values)` — invalid C.
   - Dot-access is not lowered inside free-call argument position.
3. `SPC Holder: values: LIST. /SPC.`:
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
  - Fix SPC parser crash.
  - Support map literals.

### User Guidance
- Do NOT rely on structs in production code.
- Use explicit `_map_get` / FFI where possible.
- Free struct fields manually (if supported at all).

## TD-010: Multi-digit string leak in plant_list_free — CLOSED
- **Status:** ✅ CLOSED in v0.51.2d.1
- **Fixed by:** header-tagged heap strings.
  - `_from_long` (n>=10) and `_from_double` now allocate via
    `plant_heapstr_alloc_copy` (8-byte magic prefix).
  - `plant_list_free` and `plant_mem_free` call `plant_heapstr_release`.
- **Runtime exception:** this is a documented runtime change (Principle GGG).
- **Regression fixed:** `plant_mem_free` now releases tagged strings
  (previously `free()` on a tagged pointer → abort).

## TD-011: COUNT in complex expressions — RECLASSIFIED

- **Status:** TRANSLATION CORRECT; failures are type-system gaps.
- **Fixed in v0.51.2c.3/d.1:**
  - `SHOW COUNT(L).`
  - `IF COUNT(L) > 0, ...`
  - `CYCLE i FROM 1 TO COUNT(L), ...`
  - `COUNT(L1) + COUNT(L2)`
  - `COUNT(get())`
- **Remaining (type-system gap, not COUNT defect):**
  - Parenthesized: `SHOW (COUNT(L1) + COUNT(L2))`
  - Non-numeric-returning calls: `SHOW wrap(COUNT(L))`
- **Root cause:** `seg_is_numeric` returns 0 for a leading `(` or
  unknown-return-type calls. This is a type-inference gap. The
  `COUNT(...)` → `plant_array_length(...)` lowering itself is correct.
- **Target:** v0.52.0 (Type System Audit).
- **User guidance:** use temp variables for parenthesized arithmetic:
```plantlang
CREATE n TO COUNT(L1) + COUNT(L2).
SHOW n.
```

- **Related:** TD-007 (original COUNT fix), TD-012 (SUITE nums gap).

## TD-012: collect_nums_walk does not recurse into suite_stmt — CLOSED

- **Status:** ✅ CLOSED in v0.51.2d.1
- **Fixed:** `collect_nums_walk` now recurses into `suite_stmt` bodies.
- **Test:** `suite_nums.plant`

## TD-013: valgrind-check-tensor masks valgrind exit code — CLOSED
- **Status:** ✅ CLOSED in v0.51.2d.1
- **Fixed:** `valgrind-check-tensor` now captures and checks valgrind's
  exit code (redirect to file, then cat + exit).
- **Companion:** `valgrind-check-tensor-soft` (report-only).
- **Consequence:** `verify-v0.51.2a/b/c` gates that reference
  `valgrind-check-tensor` now fail on pre-existing leaks. This is
  correct. v0.51.2d.1 fixes the underlying leaks (TD-008 + TD-010).

## TD-014: tensor_basic test leaks its own T4/T5 — CLOSED
- **Status:** ✅ CLOSED in v0.51.2d.1
- **Fixed:** `tensor_basic` and `tensor_refcount` now free their tensors
  (test hygiene).

## TD-015: Global/cross-scope lifecycle tracking — CLOSED
- **Status:** ✅ CLOSED in v0.54.2c
- **Resolved (v0.54.2c):**
  - Item 1: auto-cleanup (LST/TPL/MAP/TENSOR/GEO).
- **Resolved (v0.54.2b):**
  - Item 4: cross-scope free (memory safety).
- **Resolved (v0.54.2a):**
  - Item 2: GLOBAL in ACTION bodies.
  - Item 3: numeric GLOBAL in ACTION-body `nums`.
- **Resolved (v0.51.3b):**
  - GLOBAL keyword, GLOBAL in SUITE/IF/CYCLE, GLOBAL_FREE,
    list mutation, init order.
- **Notes:**
  - Auto-cleanup covers LST/TPL/MAP/TENSOR + GEO.
  - QUE/STK/JSN/SET: deferred (type-specific).
  - Numeric/TXT: no free (as before).

### Related
- TD-016 (STATIC variables).
- TD-017 (closure capture lifecycle).
- TD-019 (SUITE parsing), TD-020 (list operations).

## TD-016: STATIC variables not supported — DEFERRED
- **Status:** DEFERRED to v0.52.0 (Type System Audit)
- **Introduced:** pre-v0.51.2 (long-standing; `STATIC` is not a keyword)
- **Impact:** LOW — `STATIC` is not part of the language today.
- **Symptom:** `STATIC c TO 0.` is silently dropped; subsequent uses of
  `c` emit `c undeclared` in the generated C (GCC error).
- **Root cause:** `STATIC` is not a lexer/parser keyword; the statement
  is ignored.
- **Target:** v0.52.0 — add `STATIC` declarations (C `static` storage)
  and lifecycle tracking. Static lifecycle tracking cannot precede
  STATIC language support.

## TD-017: Closure capture lifecycle not managed — DEFERRED
- **Status:** DEFERRED to v0.52.0 (Type System Audit)
- **Introduced:** v0.51.2d.2 (closure scope analysis)
- **Impact:** LOW-MEDIUM.

### Findings
1. Captured variables are stored in **heap-allocated `plant_Env_N`
   structs** via `plant_env_alloc` (NOT the codegen `env_make`
   mechanism), registered in `_env_registry`.
2. Env structs are never individually freed (they remain
   "still reachable" via the registry) → potential growth.
3. MOVE/REF capture ownership for LIST/TENSOR captures is undefined
   (MOVE copies the value, REF aliases the outer variable).
4. **Positive:** `freed_vars` DOES work inside generated closure bodies
   — a `LIST_FREE` followed by `FREE` emits a single free (verified by
   `closure_free.plant`).
5. The directive's nested-`ACTION` closure syntax is NOT supported
   (emits malformed C); real closures use `[captures](params) -> body`.

### Target
v0.52.0 — define capture ownership and free closure envs.

### User Guidance
Do not rely on captured container lifetimes; free containers in the
defining scope.

## TD-018: CHA display segfault — CLOSED
- **Status:** ✅ CLOSED in v0.51.13
- **Root cause (corrected):** the crash was in SHOW, not CREATE.
  The raw char (int 65) was passed where a tx_t pointer was
  expected.
- **Fix:** gen_show_stmt now wraps bare CHA identifiers with
  `plant_char_value(...)`.
- **Related:** TD-026.

## TD-019: Top-level SUITE terminates parsing — CLOSED
- **Status:** ✅ CLOSED in v0.51.23
- **Fixed:** parse_suite_stmt returned `_from_long(p5)` (a tx_t string)
  as the position instead of `p5` (LON).
  - 1-line fix: `_from_long(p5)` → `p5`.
- **Impact:** Multi-SUITE (and statements after) now parse correctly.
- **Bootstrap:** 1-stage (compiler source has no top-level SUITE).
- **Related:** —

## TD-020: List operations (+, [], PUSH) — CLOSED
- **Status:** ✅ CLOSED in v0.51.16
- **Added:**
  - `lst + [item]` (concat → new list)
  - `lst[i] = value` (index assignment)
  - `PUSH(lst, item)` (push keyword via function-rewrite)
- **Runtime:** plant_list_concat, plant_list_set, plant_list_push.
- **Preserved:** l.push() method syntax (PUSH via function-rewrite).
- **Related:** TD-019.

## TD-021: `IS` between variables does not lower to string compare — CLOSED
- **Status:** ✅ CLOSED in v0.51.5
- **Fixed:** `IS` now works consistently between variables.
  - String variables: strcmp-based comparison.
  - Numeric variables: numeric comparison (via nums).
  - Runtime-built strings (e.g., `"hel"+"lo" IS "hello"`): now equal.
- **Approach:** targeted pre-pass (`rewrite_cond_is`) at condition
  sites; rewrites `<id> IS <id>` → `strcmp(l, r) == 0` only when both
  operands are bare non-numeric identifiers (NULL/TRUE/FALSE excluded).
- **Historical:** literals already worked (via handle_strcmp);
  variable-vs-variable used pointer compare.
- **Preserved:** literal-vs-literal behavior unchanged.
- **Related:** TD-022.

## TD-022: DBL display truncates doubles — CLOSED
- **Status:** ✅ CLOSED in v0.51.22
- **Fixed:** DBL/FLT/DCM display uses _from_double (no truncation).
- **Approach:** collect_dbls_walk + __DBL__ markers + _expr_has_dbl.
- **Preserved:** LON/ULO still use _from_long (avoid scientific notation).
- **Related:** TD-024 (DCM portability).

## TD-024: DCM (long double) size is implementation-defined — CLOSED (documented)
- **Status:** ✅ CLOSED (documented) in v0.51.23
- **Reason:** DCM precision is inherently platform-defined. No portable
  guarantee without a specific extended type.
- **Documented:**
  - x86-64: 16 bytes (80-bit extended, stored in 16B).
  - ARM: 64-bit (same as DBL) or 128-bit.
- **Display:** Fixed in v0.51.22 (_from_double).
- **Decision:** Document as-is (Option A). No code change.
- **Related:** TD-022.

## TD-023: UNUM (unsigned long) removed — CLOSED in v0.51.9
- **Status:** ✅ CLOSED in v0.51.9
- **Reason:** Duplicated by ULO.
- **Removed:** UNUM record + any references.
- **Replacement:** ULO (unsigned long).
- **Related:** TD-022.

## TD-025: CMP arithmetic is target-typed — CLOSED
- **Status:** ✅ CLOSED in v0.52.1
- **Introduced:** v0.51.11 (CMP added).
- **Fixed:** CMP ops now operand-based (via `__CMP__` markers) + target-typed.
  - `SHOW A + B.` works (`plant_complex_add(A, B)`).
  - `SHOW Z.` (CMP var) works (via `plant_complex_to_str`).
  - `CREATE S (CMP) TO A + B.` still works (target-typed path preserved).
- **Regression caught & fixed:** real accessors (`plant_cmp_real/imag/arg/cabs`)
  guarded — they already return `tx_t` and must not be re-wrapped.
- **Related:** TD-022, TD-024.

## TD-026: TX (text alias) removed — CLOSED
- **Status:** ✅ CLOSED in v0.51.14
- **Removed:** TX record + all TX annotations (947 src + 98 tests).
- **Replacement:** TXT (canonical).
- **Related:** TD-018.

## TD-027: str_eq / handle_strcmp literal-paren bug — CLOSED
- **Status:** ✅ CLOSED in v0.51.17
- **Fixed:** handle_strcmp pass now handles literal parens inside
  str_eq arguments correctly via an escape-aware quote mask.
- **Root cause:** the LEFT operand scan (ls loop) was not quote-aware,
  unlike the right scan.
- **Approach:** Option A (quote-aware scanning).
- **Workaround removed:** _lst_binop now uses literal "(" / ")" /
  "[" / "]" / "\"" / "+".
- **Runtime change:** plant_compat.h (approved as root-cause fix).
- **Discovered by:** v0.51.16 Phase 3.

## TD-028: MAP — pair-list literal vs PlantMap* registry — CLOSED
- **Status:** ✅ CLOSED in v0.51.22
- **Fixed:** MAP registry aligned to PlantArray* (pair-list).
- **Reason:** MAP {...} literal produces pair-list (PlantArray*, kind=1).
- **Related:** TD-022.

## TD-029: Native test suite is slow (~10 min) — CLOSED
- **Status:** ✅ CLOSED in v0.52.0
- **Cause:** Test runner recompiled the full runtime (10 .c files) per test (~2.7s × 123).
- **Fix:** Precompile the test-runtime subset to build/libplant_test_runtime.a once; link per test.
- **Result:** Native suite ~10 min → 46 s (~13x faster).
- **Related:** —

## TD-030: Unknown types fall through to tx_t (silent) — CLOSED
- **Status:** ✅ CLOSED in v0.52.2
- **Introduced:** v0.51.4 (Type Registry) — `plant_ctype` fell back to `tx_t` for any unrecognized type name.
- **Impact:** `CREATE X (FOOBAR) TO 5.` compiled silently (`tx_t X = 5;`); typos surfaced only as later C errors (or not at all).
- **Fixed:** `validate_types` AST pass (codegen_c.plant) invoked from `main.plant` before codegen; rejects unknown type names in CREATE/LET/GLOBAL var_types and ACTION/EXTERNAL param + return types.
- **Whitelist:** registry types + TYPE aliases + struct/union/enum/species names + generic params + pseudo-forms (`REF`, `STR X`, `ENUM X`, `ARRAY[T,N]`, `UNION`, `MATH`, `CALLBACK`, `VOID`, `void*`, `external`, `Result<T,E>`, `STRING`, `VEIN`).
- **Error:** `Error: unknown type 'FOOBAR'.` (exit 1).
- **Incidental:** caught a latent `LIST`→`LST` typo in the self-hosting source.
- **Related:** —

## TD-031: Regression runner never links plant_memory.c — CLOSED
- **Status:** ✅ CLOSED in v0.52.3c
- **Introduced:** pre-v0.51 (`tests/regression/run_regression_tests.sh` unchanged since v0.50.0f).
- **Impact:** 304 gcc-stage link failures across the regression suite (compiler-independent).
- **Cause:** the runner linked 7 runtime `.c` files but not `plant_memory.c` / `plant_tensor.c` / `plant_complex.c`, while `plant_runtime.c` references `plant_heapstr_alloc_copy` / `plant_heapstr_release` (defined only in `plant_memory.c`).
- **Fixed:** `run_regression_tests.sh` now links `build/libplant_test_runtime.a` (the precompiled `TEST_RUNTIME_C` archive), like the native runner, with a `make test-runtime-lib` fallback.
- **After:** 310 passed / 8 failed (TD-034 residuals); suite runtime ~2m12s.
- **Related:** TD-029 (performance), TD-034.

## TD-034: Regression suite residual failures (8) — CLOSED
- **Status:** ✅ CLOSED in v0.52.3d
- **Introduced:** pre-v0.51 (long-standing; masked by TD-031).
- **Impact:** MEDIUM — 8 tests failed after the TD-031 fix.
- **Fixed:**
  - `json_http`: `JSN` added to the `parse_harvest_stmt` `okws` list + a `JSN` branch.
  - `map_literal`: `_lst_binop` now tracks `{}` depth (a `+` inside a map literal was mistaken for a list concat).
  - `math_extra`/`math_fix`/`math_full`: `math_PI`/`math_E` now emit `()` via `_fix_math_consts` (boundary-guarded).
  - `string_ops`: `_handle_cast` is now string-aware (the real bug — a literal `@` inside a string was treated as the cast operator; not the trailing-`#` hypothesis).
  - `storm_factory`/`storm_shelter`: `.expected` regenerated.
- **Result:** Regression **318/318**.
- **Related:** TD-031.

## TD-035: storm_* .expected are line-number-brittle — CLOSED
- **Status:** ✅ CLOSED in v0.52.7
- **Fixed:** regression runner normalizes generated-C file/line metadata
  (`norm()`; gated to `storm_*`), and the two `.expected` files use
  placeholders (`<gen>`, `<n>`).
- **Preserved:** non-empty / non-zero assertions (`[1-9][0-9]*`).
- **Related:** TD-034.

## TD-032: SPC field list SIGSEGV (comma + EOF) — CLOSED
- **Status:** ✅ CLOSED in v0.52.3b
- **Introduced:** v0.49.28 (`parse_species_decl`).
- **Impact:** `SPC H { x: LON, y: LON }` (comma), comma + embedded `ACTION`, and unterminated `SPC H { x: LON` all crashed with **SIGSEGV (exit 139)**.
- **Cause:** the species field loop consumed the type as a single token and never handled a trailing `,`; and it had no EOF guard (unlike `parse_struct_decl`), so any mis-parse walked off the token array.
- **Fixed:** `parse_species_decl` now (1) consumes an optional `,` between fields, and (2) has an `is_eof_at` guard mirroring `parse_struct_decl`.
- **After:** graceful termination; `SPC H { x: LON, y: LON }` compiles and runs.
- **Related:** TD-009.

## TD-033: Species fields default to "" with explicit ACTION main() — CLOSED
- **Status:** ✅ CLOSED in v0.52.3g
- **Introduced:** pre-v0.52.3b (observed during v0.52.3b investigation).
- **Impact:** MEDIUM — programs with an explicit `ACTION main()` never emit `plant_species_register(...)`, so species fields default to `""` at runtime (the map is only populated via `.put`).
- **Cause:** deeper than first recorded: `parse_program` **dropped** every `species_decl` node (only its lifted methods survived), so `sp_init_code` was always empty — for *every* main path, not just the action-main path.
- **Fixed (v0.52.3g):**
  - `parse_program` keeps the `species_decl` node (metadata only).
  - `parse_species_decl` now stores the `IMPLEMENTS` list as `"implements"`.
  - Codegen's malformed `plant_list_make(...)` build (dead path) rewritten.
  - Explicit `ACTION main()` carries the registration prologue when the
    int-main wrapper is not emitted; the wrapper path is unchanged.
- **Related:** TD-032, TD-009.

## TD-037: Tail return emits `return <fnname>;` — CLOSED
- **Status:** ✅ CLOSED in v0.52.3h
- **Fixed:** line 7491 → `return "";` (was `return <fnname>;`).
- **Before:** non-GIVE tails emitted `return <fnname>;` (function
  designator returned as tx_t).
- **Related:** —

## TD-036: Dotted-decl `ACTION Type.method()` broken — CLOSED
- **Status:** ✅ CLOSED in v0.52.4
- **Fixed:** `parse_action_decl` consumes `.method`; rewrites to bare
  method name with `self` (TXT) receiver param.
- **Related:** TD-009.

## TD-038: `STR X` param maps to C struct by value (FFI semantics) — CLOSED
- **Status:** ✅ CLOSED in v0.52.5
- **Fixed:** non-FFI (`action_decl`) `STR X` param → `tx_t` (map handle).
  - `action_param_ctype` (def + forward decl).
  - `is_ffi_callee` gating (call-arg + return marshalling).
- **Preserved:** FFI (`external_decl`) `STR X` → `struct_val` (all 6 FFI tests pass).
- **Related:** TD-009.

## TD-039: non-FFI `-> STR X` (multi-token return) mis-parses — CLOSED
- **Status:** ✅ CLOSED in v0.52.6
- **Fixed:** `parse_action_decl`'s `ret_lx IS "STRUCT"|"STR"` branch now
  uses bounded collection (IDENT + optional `[...]` group) instead of
  the greedy `collect_type_text(p5, ".", 0)`.
- **Preserved:** FFI (`external_decl`) `-> STR X.` unchanged.
- **Related:** TD-038.

## TD-040: GEO SHOW substring (CMP hook same issue) — CLOSED
- **Status:** ✅ CLOSED in v0.54.1
- **Fixed:**
  - GEO (v0.54.0): `is_bare_id` guard.
  - CMP (v0.54.1): `is_bare_id` OR `plant_complex_` substring.
- **Preserved:** CMP arithmetic (`SHOW A + B`).
- **Related:** TD-025.

## TD-041: Tensor intermediates leak — CLOSED (common cases)
- **Status:** ✅ CLOSED in v0.57.0c (common cases)
- **Introduced:** v0.55.0 (tensor ops)
- **Fixed:** Nested tensor-op calls in a self-contained `CREATE`/`SET`
  RHS are hoisted to temps and freed after assignment.
- **Coverage:** `CREATE c (TENSOR) TO TENSOR_ADD(TENSOR_MUL(a,b), b)`
  and similar (≤3+ levels, matmul).
- **NOT covered:** Escapes (`GIVE`, `PUSH`, map/struct, closures, FFI) → TD-044.
- **Approach:** Narrow Option A — fail-safe recursive hoist with an
  allow-list of 16 `PlantTensor*`-returning ops. (Not Option E — a temp
  registry is unsound because of escapes.)
- **Related:** TD-042, TD-044, TD-045.

## TD-044: Tensor intermediates leak on escape paths — CLOSED (GIVE + map/array)
- **Status:** ✅ CLOSED in v0.57.1 (GIVE default form + map/array element)
- **Introduced:** v0.57.0c (discovered during the TD-041 investigation)
- **Fixed:** Nested tensor-op expressions in:
  - `GIVE` (ACTION return, default form only; not async/`mexit`).
  - `SET M[k] TO …` / `SET L[k] TO …` (map/array element).
  Both hoisted via `_hoist_tensor_rhs` + freed after escape.
- **NOT covered:** `PUSH` (via `gen_reap_stmt` — HIGH risk) → TD-046.
- **NOT covered:** closures (TD-017), FFI stores → TD-046.
- **Related:** TD-017, TD-041, TD-045, TD-046, TD-047.

## TD-046: PUSH nested intermediates leak — CLOSED

- **Status:** ✅ CLOSED in v0.58.12
- **Introduced:** v0.57.1 (discovered during the TD-044 investigation)
- **Symptom:** `PUSH(L, TENSOR_ADD(TENSOR_MUL(a, b), b))` emitted
  `plant_list_push(L, plant_tensor_add( plant_tensor_mul(a, b), b))` —
  the inner `plant_tensor_mul` result leaked (**72 B / 1 block**). The
  pushed *outer* tensor is owned by the list (`LIST_FREE` releases it), so
  only nested intermediates leaked (single op / bare var = 0 B).
- **Fixed:** a **fail-safe** interception in `generate_node`'s `call_stmt`
  branch (`codegen_c.plant`, +40 lines):
  - Translate the PUSH value, run `_hoist_tensor_rhs` (the proven
    v0.57.0c hoister) and, **only when it produces temps**, emit
    `{ PlantTensor* _h… = …; plant_list_push(L, <outer>); plant_tensor_free(_h…); }`.
  - Otherwise **fall through** to the existing `reap_stmt` delegation
    unchanged (single-op, bare var, numeric, closures/SAFE/FFI).
  - `gen_reap_stmt` (the 249-line HIGH-risk emitter) is **untouched**.
  - The outer tensor is never freed here — ownership stays with the list.
  - 3-level nesting works (multiple temps hoisted + freed).
- **Milestone:** with TD-046 closed there are **0 known bugs open**
  (0 silent-wrong, 0 crash, 0 leak). Remaining open TDs are **features**
  (TD-048, TD-055) and 2 DEFERRED items (TD-016, TD-017).
- **Tests:** `td046_push_nested`, `td046_push_single_unchanged`.
- **Related:** TD-041 (intermediate leak, closed), TD-044 (GIVE, closed),
  TD-017 (deferred).

## TD-049: `SHOW <multi-digit long>` leaks `_from_long` heap string — CLOSED

- **Status:** ✅ CLOSED in v0.58.2
- **Introduced:** pre-v0.58.1 (discovered during v0.58.1 Phase 1)
- **Impact:** LOW (~13 B per `SHOW <long>`; same class as TD-043)
- **Symptom:** `SHOW 5000` → 13 B definitely lost (42 B for 3 SHOWs).
- **Reason:** `plant_iReport_print` does not free the multi-digit
  `_from_long` heap string (only 0–9 are static).
- **Fixed:** `gen_show_stmt` wraps numeric `SHOW` in
  `{ tx_t __show = <num>; print(__show); plant_heapstr_release(__show); }`.
  Release is magic-checked, so it is safe on prefixed static and
  non-tagged pointers.
- **Precondition:** `_from_digit` 0–9 now uses a prefixed 10-slot static
  table (`p-8` safe, no rotation, stable pointers).
- **Rejected:** v0.58.2a made `_from_long` a rotating buffer → UNSOUND
  (compiler corruption; reverted). `_from_long` values are long-lived
  inside the compiler.
- **Related:** TD-043 (sibling for doubles).

## TD-051: Bare float literal not `_from_double`-wrapped — CLOSED

- **Status:** ✅ CLOSED in v0.58.4
- **Introduced:** pre-v0.58.2 (discovered during v0.58.2 Phase 1)
- **Broadened:** v0.58.3 (was SHOW-only; shown to affect GIVE too)
- **Fixed:** new `has_float_lit` helper (quote-aware, base-char checked)
  plus guarded wraps in `gen_show_stmt` (after the TD-022 `_expr_has_dbl`
  site) and `gen_give_stmt` (after `isn3`). String literals (`"5.0"`)
  and already-wrapped values (`TENSOR_SUM`/`TENSOR_AT`, detected via
  `COUNT … "_from_double("`) are never wrapped.
- **Now works:** `SHOW 5.0`, `SHOW 5.0 + 1.0`, `SHOW -5.0`, `GIVE 5.0`.
- **Bug found during the fix:** the first quote toggle reset on the
  opening quote (string contents scanned as code) — caught by the
  compiler's own `SHOW "Chloroplast X …"` build; fixed with an explicit
  `hfl_opened` flag.
- **Coupling (TD-052):** the GIVE wrap works only because every ACTION
  returns `tx_t` today; when `-> DBL` emits `double f()`, the GIVE wrap
  must be removed.
- **Tests:** `td051_show_float`, `td051_show_float_expr`,
  `td051_show_negative`, `td051_show_string_safe`,
  `td051_give_float_lon`.
- **Related:** TD-049, TD-052, TD-056.

## TD-050: `TENSOR([...])` / float literal inside `GIVE` mis-parses — CLOSED

- **Status:** ✅ CLOSED in v0.58.3
- **Introduced:** pre-v0.58.1 (discovered during v0.58.1 Phase 2)
- **Impact:** MED (blocked TENSOR/list literals with floats in ACTIONs)
- **Symptom:** `GIVE TENSOR([5.0, 6.0])` emitted
  `plant_list_make(2, 5_map_get(5, "0"), ...)`.
- **Root cause:** `_lower_dots` (parser.plant, **GIVE-only** textual
  rewrite) converted `BASE.IDENT` → `_map_get(BASE,"IDENT")` whenever the
  chars adjacent to the dot were identifier chars — digits included. The
  lexer already emits `5.0` as a single NUMBER token, so the trigger was
  the textual pass, not the lexer or the ACTION context.
- **Fixed:** guard in `_lower_dots` — skip the rewrite when the gathered
  base **starts with a digit** (float literal). +12 lines, parser only.
- **Preserved:** `x.y` / `x1.5` field access, and the `GIVE 0.` terminator.
- **Tests:** `td050_give_tensor_float`, `td050_give_list_float`,
  `td050_give_field_regression`, `td050_give_terminator`.
- **Related:** TD-046 (GIVE literal ownership), TD-051 (bare float wrap).

## TD-052: CREATE/LET numeric coercion from `tx_t` action calls — CLOSED

- **Status:** ✅ CLOSED in v0.58.5
- **Introduced:** pre-v0.58.3 (re-scoped during v0.58.5 Phase 1)
- **Premise correction:** **every** ACTION emits `tx_t f(...)` — the
  uniform text ABI (`rty` is never used for the C signature). The
  original title "`ACTION -> DBL` emits `tx_t`" was a misdiagnosis;
  `-> LON` behaves identically.
- **Real symptom:** `CREATE x (DBL) TO f()` emitted
  `plant_scl_create(_from_double(f ( )))` — `_from_double` received a
  `tx_t` → gcc type error.
- **Fixed:** CREATE/LET with a **DBL/FLT/DCM** target and an
  **action-call** RHS emit `plant_scl_create(<cval>)` directly.
  Detection: new `is_action_call(cval, sigs, want)` (sig `type ==
  "action_decl"` and, when `want` is non-empty, `ret == want`).
  Real-double extern calls (`TENSOR_SUM`, 34 corpus tests) unchanged.
- **Related:** TD-054 (LON half, still open), TD-057.

## TD-053: `SHOW <MAP>` segfaults — CLOSED (misdiagnosis)

- **Status:** ✅ CLOSED in v0.58.6 (misdiagnosis; mirror TD-047 precedent)
- **Reality:** `SHOW <MAP>` **works** — prints `{k = 42, j = 99}`.
  A MAP is a `PlantArray` with `kind = 1` (`plant_map_create` →
  `plant_list_create(8)`), so it hits the existing array path, and
  `plant_to_string` already has a map branch (`plant_runtime.c:3013`).
- **The earlier "crash":** it was **TD-054** — the probe also contained
  `_to_long(f(m))`, where `f` (`-> LON`, `GIVE M.k`) returns raw long
  bits → `strtol(0x2A)` → SIGSEGV. Output loss was stdio block-buffering.
- **Optional hardening (landed):** low-pointer guard in
  `plant_iReport_print` (`(uintptr_t)message < 4096` → `"<null>"`).
  Defensive; no output change for valid inputs. An *uninitialized* MAP
  is NULL and now prints `<null>` instead of silently skipping.
- **Tests:** `td053_show_map_ok`, `td053_show_uninit_map_null`.
- **Related:** TD-054.

## TD-054: `-> LON` raw-long return / pointer-as-long — CLOSED

- **Status:** ✅ CLOSED in v0.58.7
- **Introduced:** pre-v0.58.3 (discovered during v0.58.3 Phase 3)
- **Symptom:** `ACTION f(...) -> LON` + `GIVE M.k` emitted
  `return _to_long(...)` (raw long bits) and callers emitted
  `long v = f();` — `_to_long(f())` later → `strtol(0x2A)` → SIGSEGV.
- **Fixed (2 gated changes in `codegen_c.plant`):**
  1. **callee** (`gen_give_stmt`): `rty IS "LON"` + lookup →
     `_from_long(_to_long(<cval>))` (text).
  2. **caller** (CREATE/LET `LON`):
     `is_action_call(cval, sigs, "LON")` → `_to_long(<cval>)`.
  Both gates require an **explicit** `-> LON` (`rty == "LON"` /
  `ret == "LON"`), so **implicit-return** actions (`rty == ""`) keep the
  documented raw-bit convention — the async suite stays 6/6 green.
- **Leak-for-correctness trade:** multi-digit `-> LON` returns leak the
  `_from_long` heap string (~11 B) — a **pre-existing** action-return
  ownership gap (`tx_t` text is never released by the caller; the same
  leak exists for the literal form `GIVE 42`). 0–9 use the static
  `_from_digit` path (0 B).
- **Tests:** `td054_give_lon_lookup`, `td054_create_lon_action`,
  `td054_implicit_return_unchanged`.
- **Related:** TD-059 (misdiagnosis → TD-060).

## TD-055: ACTION bodies cannot see globals (no capture) — OPEN

- **Status:** OPEN
- **Introduced:** pre-v0.58.3 (discovered during v0.58.3 Phase 3)
- **Impact:** LOW–MED (globals inaccessible inside ACTIONs)
- **Symptom:** a global variable referenced inside an ACTION body emits a
  C reference to an undeclared name (e.g. `'m' undeclared`) — globals are
  not captured into ACTION bodies.
- **Workaround:** pass globals as parameters.
- **Target:** v0.58.4+ (or a broader scope/globals epic)
- **Related:** none.

## TD-057: `GIVE <DBL value>` silent truncation / type error — CLOSED

- **Status:** ✅ CLOSED in v0.58.5
- **Introduced:** pre-v0.58.5 (discovered during v0.58.5 Phase 1)
- **Symptom:**
  - `GIVE Y` (DBL param) → `_from_long(Y)` → **silent truncation**
    (1.5 → `"1"`).
  - `GIVE TENSOR_SUM(P)` → `return plant_tensor_sum(P);` → raw double
    from a `tx_t` function → gcc type error.
- **Fixed:** `gen_give_stmt` wraps `_from_double` when the action's `rty`
  is DBL/FLT/DCM, unless the value is a string literal, already starts
  with `_from_`, or is a call to any action (`tx_t`).
- **Related:** TD-052.

## TD-056: `_lower_dots` quote-toggle flaw — CLOSED

- **Status:** ✅ CLOSED in v0.58.6
- **Introduced:** pre-v0.58.4 (discovered during v0.58.4 Phase 3)
- **Fixed:** explicit `ld_op` opening-quote flag (mirror `has_float_lit`'s
  `hfl_opened`). Quoted regions are copied verbatim and never scanned for
  dots, so `GIVE "a.b.c"` now yields `return "a.b.c";`.
- **Preserved:** `x.y` / `x1.5` field lowering and the `GIVE 0.`
  terminator.
- **Test:** `td056_give_dotted_string`.
- **Related:** TD-051.

## TD-058: `_expr_has_dbl` substring scan (not string-literal aware) — CLOSED

- **Status:** ✅ CLOSED in v0.58.6
- **Introduced:** pre-v0.58.5 (discovered during v0.58.5 Phase 3)
- **Fixed:** new `_strip_quoted(e)` helper (quote state machine) removes
  quoted regions; `_expr_has_dbl` splits on the stripped text instead of
  the raw `cval`.
- **Now:** `SHOW "extern"` / `SHOW "x"` with `x (DBL)` stay plain strings
  (no `_from_double` wrap); `SHOW x` still wraps → `1.5`.
- **Test:** `td058_show_string_with_dbl_name`.
- **Related:** TD-051.

## TD-059: ACTION `-> STR` drops string — CLOSED (misdiagnosis)

- **Status:** ✅ CLOSED in v0.58.7 (misdiagnosis; mirror TD-053/TD-047)
- **Reality:** `STR` is the **FFI struct keyword**
  (`STR Point { x: LON, y: LON }`, `ACTION f() -> STR Point`). The
  **string** type is `TXT`. `ACTION f() -> STR` **without a struct name**
  is malformed: the parser consumes `GIVE` as the struct name and
  swallows the body → the generator emits its default `return "";`.
- **Evidence:** with two body statements the body survives
  (`SHOW "inside". GIVE "a".` → `return "a";`); with a single `GIVE`
  the body is eaten. `STR` is not in the type registry (0 occurrences in
  `codegen_c.plant`).
- **Actual defect:** the parser should **diagnose** malformed `-> STR`
  instead of silently swallowing the body → filed as **TD-060**.
- **Workaround:** use `-> TXT` for string returns.
- **Related:** TD-060.

## TD-060: `parse_action_decl` should reject `-> STR` without struct name — CLOSED

- **Status:** ✅ CLOSED in v0.58.8
- **Introduced:** pre-v0.58.7 (discovered during v0.58.7 Phase 1)
- **Fixed (2 parts):**
  1. **Parser** (`parse_action_decl`): peek the next token **type**; if it
     is not `"IDENT"` the return is set to `"STR"` and the token is **not
     consumed** — the ACTION body now parses normally instead of being
     swallowed.
  2. **Validator** (`_known_type`): the `"STR "` branch now requires the
     base name (before `[`) to be a **declared** type
     (`list_contains names base`). Previously every `STR <anything>` was
     accepted.
- **Error channel:** the existing `unknown type '<rt>'` path
  (`validate_types_walk` → `main.plant`), TD-030 style; no codegen
  happens on failure.
- **Verified:**
  - `-> STR` → `Error: unknown type 'STR'.` (rc=1), was silent.
  - `-> STR Nonsense` → `Error: unknown type 'STR Nonsense'.` (rc=1).
  - `-> STR Point` (declared) → compiles, unchanged.
  - `-> TXT` → unchanged.
- **Tests:** `td060_str_point_valid`, `td060_txt_unchanged`.
- **Convention note:** `tests/native/` has no expected-error support; the
  `tests/regression/` runner does (it greps the compile log for each
  `.expected` line), so gated error assertions belong there.
- **Related:** TD-059 (misdiagnosis — the original report).

## TD-061: `SET <obj>.<field> TO <expr>` mis-lowers — CLOSED

- **Status:** ✅ CLOSED in v0.58.9
- **Introduced:** pre-v0.58.8 (discovered during v0.58.8 Phase 3)
- **Symptom:** `SET p.x TO 1.` emitted `p = x TO 1;` (gcc: `'x'
  undeclared`) — `parse_set_stmt` consumed the `.` as the assignment
  operator (it never validates that token) and collected `x TO 1` as the
  value.
- **Fixed (2 parts):**
  1. **Parser** (`parse_set_stmt`): new AST key `"field"` (bare name) when
     `.` + `IDENT` follows the target, guarded by `sidx == ""` (no clash
     with `[index]`). A non-IDENT after `.` falls through unchanged.
  2. **Codegen** (`gen_set_stmt`): new branch **before** the index branch →
     `<id> = plant_map_set(<id>, "field", <value>);` (upsert).
- **Numeric wrap:** mirrors the index path (`isnf IS 1 → _from_long`).
- **Value-side reads:** `SET p.x TO p.x + 5` already lowers via the
  existing value path (`_map_get(p, "x") + 5`).
- **Known gaps:**
  - TD-041/044 tensor-RHS hoisting is not applied on field writes
    (0 corpus cases).
  - **FFI by-value structs** (`STR X` declared for `external` FFI) are real
    C structs, not map-backed; their field access (read *or* write) needs a
    separate path. The fix is consistent with the existing (map-backed)
    field **read** lowering.
- **Tests:** `td061_set_obj_field`, `td061_set_obj_field_expr`.
- **Related:** TD-062 (validate assignment operator), TD-063 (bracket map
  element set silent no-op).

## TD-062: `parse_set_stmt` should validate the assignment operator — CLOSED

- **Status:** ✅ CLOSED in v0.58.10
- **Introduced:** pre-v0.58.9 (discovered during v0.58.9 Phase 1)
- **Symptom:** `parse_set_stmt` blindly consumed **any** token as the
  assignment operator (the reason `.` was silently swallowed in TD-061;
  also `SET x BLAH 42` silently compiled to `x = 42;` and `SET x 42`
  emitted invalid C `x = ;`).
- **Fixed (2 parts):**
  1. **Parser** (`parse_set_stmt`): `badop` marker when the operator
     token type is neither `"TO"` nor `"="` (still consumes the token so
     parsing continues — the parser has no error channel).
  2. **Validator** (`validate_types_walk`): new `set_stmt` case →
     `SET '<target>': expected 'TO' or '=' after the target`.
- **Verified:**
  - `SET x TO 42` / `SET x = 42` → work.
  - `SET x BLAH 42` / `SET x + 42` / `SET x 42` →
    `Error: SET 'x': expected 'TO' or '=' after the target.` (rc=1).
- **First regression error test:** `td062_set_badop` (`.invalid` +
  `.expected`).
- **Related:** TD-061.

## TD-063: `SET M["k"] TO <value>` silently no-ops — CLOSED

- **Status:** ✅ CLOSED in v0.58.10
- **Introduced:** pre-v0.58.9 (discovered during v0.58.9 Phase 2)
- **Symptom:** `SET M["k"] TO 42.` emitted
  `plant_list_set(M, "k", _from_long(42));` → **no-op** (`plant_list_set`
  takes an `int64_t`, so the string key became a pointer address → out of
  range → silent early return).
- **Fixed:** `gen_set_stmt`'s index branch — if `cidx` starts with `"`
  (a string literal key), emit
  `target = plant_map_set(target, "key", <wrapped>);` (mirroring the
  TD-061 numeric wrap). Numeric indices (`SET L[0]`) keep
  `plant_list_set` and the TD-044 hoisting path unchanged.
- **Verified (write side):** `SET M["k"] TO 42` → `SHOW M` = `{k = 42}`,
  `SHOW M.k` = `42`; `SET M["k"] TO 5 + 10` → `{k = 15}`.
- **Limitations:** variable keys (`SET M[k]`) stay on the list path
  (statically ambiguous); map writes skip TD-044 tensor-RHS hoisting
  (same documented gap as TD-061); `SET L["0"]` (nonsense) would upsert
  and set `kind = 1`.
- **Not covered:** the **read** side (`M["k"]`) — TD-064.
- **Tests:** `td063_set_map_bracket`.
- **Related:** TD-061, TD-064.

## TD-064: `M["k"]` map bracket read silently broken — CLOSED

- **Status:** ✅ CLOSED in v0.58.11
- **Introduced:** pre-v0.58.10 (discovered during v0.58.10 Phase 2)
- **Severity:** was **P1** (silent-wrong empty string) for single-key
  reads and **P2** (SIGSEGV) for nested reads (`M["a"]["b"]` because the
  inner `plant_list_get` returned `""` and the outer one dereferenced it).
- **Fixed:** `handle_brackets` (`runtime/c/plant_compat.h:315`, the static
  C helper invoked from `translate_expr` after `_list_literal`) now emits
  `_map_get(base, key)` when the index text starts with `"` or `'`
  (string literal) instead of `plant_list_get(base, idx)`. ~8 lines.
  Mirrors the TD-063 write side (`plant_map_set` for string-key bracket
  writes, v0.58.10).
- **Preserved:**
  - Numeric list indices (`L[0]` → `plant_list_get`) — unchanged.
  - List literals (`["…"]`) are transformed by `_list_literal` **before**
    `handle_brackets` → untouched (bootstrap inert).
  - Multi-pass chaining; call-result indexing (`f(x)["k"]`) works.
- **Bonus:** `M["k"] + 1` now coerces numerically (`43`) instead of
  concatenating an empty string with `1`.
- **Tests:** `td064_map_bracket_read`, `td064_map_bracket_nested`,
  `td064_list_numeric_unchanged`.
- **Related:** TD-063 (write side, closed v0.58.10).

## TD-048: Higher-order AD — CLOSED (1st/2nd complete, 3rd by design)

- **Status:** ✅ CLOSED in v0.60.0d (re-scoped)
- **Scope (final):**
  - **1st order** — complete (`x² → 2x`, `x³ → 3x²`, `sum → 1`, …).
  - **2nd order** — complete (`x² → 2`, `x³ → 6x`, `sum → 0`, …).
  - **3rd+ order** — unsupported **by design** (explicit error gate).

### Why 3rd+ order is unsupported (design decision)
No mainstream ML technique needs 3rd-order derivatives:

| Application | Order | Status |
|---|---|---|
| SGD / Adam / AdamW / RMSprop | 1st | ✅ |
| Momentum / Nesterov | 1st | ✅ |
| Newton's method | 2nd | ✅ |
| Gauss-Newton / Levenberg-Marquardt | 2nd | ✅ |
| L-BFGS | 2nd | ✅ |
| PINNs (physics-informed NNs) | 2nd | ✅ |
| MAML (meta-learning) | 2nd | ✅ |
| Hessian-vector products | 2nd | ✅ |
| Natural gradient | 2nd | ✅ |
| 3rd-order optimizers | 3rd | ❌ none in production |
| Higher-order PDEs (KdV, Boussinesq) | 3rd+ | ❌ academic niche |

The cost of ADv2 v2 (node-keyed gradients, ~200–300 lines, HIGH risk)
is not justified by a nonexistent use case.

### What is delivered (0a + 0b + 0c)
- **0a (v0.60.0a)** — ADv2 isolated module (`plant_ad2.{c,h}`), chained
  hook (first-order `GRAD` untouched), `GRAD_HO(loss, w)` API,
  8 EASY + MUL ops, 1st/2nd order verified.
- **0b (v0.60.0b)** — 7 MID ops (DIV, MATMUL, RELU, SIGMOID, TANH, GELU,
  SWISH, MISH), scratch-list discipline (0 leaks), level-≥3 gate.
- **0c (v0.60.0c)** — SOFTMAX (1-D + 2-D, taped Jacobian, validated
  against the old engine: `[-0.25, 0.25]`), broadcast MUL/DIV,
  `a2_keep_sum`.
- **Tests** — 30+ ADv2 tests, all leak-free.
- **Semantics** — `GRAD_HO` is **single-gradient-per-forward-pass**
  (use one `GRAD_HO` per forward pass, or re-run the forward).

### The gate (explicit error, no silent-wrong)
```
GRAD_HO(GRAD_HO(GRAD_HO(...)))
  → ad2: 3rd+ derivatives unsupported (single-DAG pending 0c)
```
The user learns immediately — there is **no silent-wrong answer**.

### How to enable 3rd order if ever needed
ADv2 v2 (node-keyed gradients, PyTorch-style) is the only correct path:
1. design doc + prototype in `/tmp` (as done for the v0.60.0d diagnosis);
2. replace the engine with the 292-test baseline as the guard.
Target: **v0.61.x+** epic, only on a real use case.

**Do not attempt partial fixes** (C+B, target-link, …) — they are
disproven by design (see the v0.60.0d investigation).

### Related
- `docs/ADV2_LIMITATIONS.md` — detailed analysis.
- `docs/CHANGELOG.md` v0.60.0a/b/c — what was delivered.
## TD-047: MAP value ownership — CLOSED (misdiagnosis, v0.58.0)
- **Status:** ✅ CLOSED (misdiagnosis, v0.58.0)
- **Introduced:** pre-v0.57.1 (discovered during the TD-044 investigation)
- **Original claim:** `SET M["k"] TO a` + `LIST_FREE(M)` leaks 72 B.
- **Correction (empirical):** the 72 B was the caller's **un-freed `a`**.
  `LIST_FREE(M)` does not free map values (alias-only by design);
  `TENSOR_FREE(a) + LIST_FREE(M)` = 0 leaks, no double free. **No fix needed.**
- **Return-aliasing half:** fixed in v0.58.0 by `GIVE <bare var>` retain.
- **Related:** TD-044, TD-046.

## TD-045: `SET x TO <new tensor>` overwrites old x without freeing — CLOSED
- **Status:** ✅ CLOSED in v0.58.0
- **Introduced:** pre-v0.57.0c (discovered during TD-041 Phase 2)
- **Previously:** 🔶 WONTFIX in v0.57.2 (aliasing hazard made
  free-before-overwrite unsafe).
- **Fixed:** `SET x TO <tensor expr>` now frees the old `x` in **both** the
  plain path and the TD-041 hoist path. This is safe now that
  `CREATE <bare var>` and `GIVE <bare var>` retain on alias
  (`plant_tensor_retain_if_tensor`).
- **Coverage:** fresh `plant_tensor_*` RHS and nested (hoist) RHS.
  `SET x TO <bare tensor var>` remains unowned → TD-046.
- **Related:** TD-044, TD-046, TD-047.

## TD-042: TENSOR([...]) literal input-list leak — CLOSED
- **Status:** ✅ CLOSED in v0.57.0b
- **Introduced:** pre-v0.56.0a (discovered in v0.56.0a Phase 5 valgrind)
- **Fixed:** `CREATE (TENSOR) TO TENSOR([...])` now frees the input list
  (mirrors SHOW's TD-008 safe block).
- **Approach:** E — fix the `gen_create_stmt` emission directly; **no**
  `plant_tensor_from_list_take` (the v0.56.0c `_take` attempt bypassed
  TD-008 and was reverted).
- **Coverage:** 85 CREATE sites (61 files); `TENSOR(<var>)` is guarded
  (arg is not `plant_list_make(`) and keeps caller ownership.
- **Related:** TD-008, TD-041.

## TD-043: SHOW `<double>` leaks `_from_double` string — CLOSED
- **Status:** ✅ CLOSED in v0.57.0a
- **Introduced:** pre-v0.56.0c (discovered in v0.56.0c Phase 4 valgrind)
- **Fixed:** `_from_double` uses a 64-slot static rotating buffer with a
  64-byte zeroed header prefix (safe with `plant_heapstr_release`, which
  no-ops on magic mismatch).
- **Impact:** ~24 B per `SHOW <double>` closed.
- **Related:** TD-042, TD-003.
