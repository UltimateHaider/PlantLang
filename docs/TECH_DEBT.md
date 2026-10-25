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

## TD-046: Tensor intermediates leak on PUSH/closures/FFI — OPEN
- **Status:** OPEN
- **Introduced:** v0.57.1 (discovered during the TD-044 investigation)
- **Impact:** LOW (rare)
- **Symptom:**
  - `PUSH(L, <nested tensor-op>)` — routes through `gen_reap_stmt`
    (HIGH-risk emitter: closures, generics, SAFE, FFI).
  - Closure capture of a nested tensor-op expression (TD-017).
  - FFI stores the tensor pointer (unknowable).
  - `GIVE <caller tensor>` aliasing (double-free hazard; observed Phase 4).
- **Reason:** PUSH via `call_stmt`/`gen_reap_stmt` is HIGH risk; closures
  need TD-017 lifecycle; FFI ownership unknowable; `GIVE A` aliasing is a
  pre-existing ownership issue.
- **Workaround:** Name the intermediate before PUSH/capture/FFI; return a
  **fresh** tensor (`TENSOR_ADD(...)`) rather than a caller pointer.
- **Target:** v0.58.0+ (dedicated release for PUSH + closures + FFI).
- **Related:** TD-017, TD-044, TD-047.

## TD-047: MAP value ownership + return-aliasing — OPEN
- **Status:** OPEN
- **Introduced:** pre-v0.57.1 (discovered during the TD-044 investigation)
- **Impact:** LOW–MED (per map store; per aliased return)
- **Symptom:**
  - `SET M["k"] TO a` followed by `LIST_FREE(M)` leaks the stored tensor
    (72 B). `plant_list_free` does not release tensor values stored via map
    keys, and `MAP_FREE` does not exist.
  - `GIVE <caller tensor>` returns an aliased pointer; caller and callee
    both own it → double-free risk (observed in the Phase-4
    `td044_give_non_nested` deviation).
- **Reason:** Map/struct value ownership is not managed; `GIVE` does not
  retain on return.
- **Workaround:** Use LST arrays for tensor storage in tests; return fresh
  tensors from ACTIONs (not caller pointers).
- **Target:** v0.58.0+ (ownership/lifetime epic).
- **Related:** TD-044, TD-046.

## TD-048: True higher-order AD unsupported — OPEN (hardened v0.57.4)
- **Status:** OPEN (hardened; explicit error)
- **Introduced:** v0.57.3 (investigation confirmed the backward is non-differentiable)
- **Hardened:** v0.57.4 (silent-wrong → clean error)
- **Impact:** MED (PINNs, MAML, Hessians unavailable)

### Interim hardening (v0.57.4)
- Attempting a second derivative (`GRAD(GRAD(loss, x, 1), x, 1)`) now
  returns an explicit error:
  `grad: loss does not match retained tape (second derivatives unsupported)`.
- Mechanism: `loss_d` vs `head->scalar` comparison (bit-identical, no epsilon).
- Repeated first-order still works.
- **No silent-wrong-answer.**
- `grad_second_derivative_unsupported` regression-locks the explicit-error
  behaviour.

### ⚠️ CRITICAL HAZARD: SILENT-WRONG-ANSWER (historical, v0.57.3)
- **The hazard:** with `retain=1`, `GRAD(TENSOR_SUM(dx), x, 1)` does
  **not** error. The retained tape is non-empty, so it re-seeds the tape
  head and **silently re-returns the first derivative** (`ddx == dx`),
  not the second derivative (`d²(x²)/dx² = [2, 2]`).
- **No error, no crash — silently wrong.** This is the worst failure mode.
- **Detection:** none today.
- **Workaround:** do not attempt second derivatives with `retain=1`; there
  is no correct path until the differentiable-backward epic.
- **Regression-locked by:** the `grad_second_derivative_unsupported` test
  (SHOWs `dx=[2,4]` and `ddx=[2,4]` with an explanatory comment).

### Root cause
- Backward uses raw loops + `plant_tensor_create` (no tape hooks).
- `dx` is detached (`requires_grad=0`, no tape node).
- The tape records only the forward pass.
- With `retain=1`, the retained forward tape is re-seeded (head reused) and
  backward replays the same first-order computation.

### Fix direction
- Rewrite the backward with taped ops + `requires_grad` on grads +
  double-backward rules per op (matmul, softmax Jacobian, …).
- 2× tape size, ~2–3 weeks, HIGH risk.
- Alternative: forward-mode dual numbers (separate implementation).

### Interim hardening (recommended follow-up)
- Runtime guard: when `retain=1` and the root is the retained tape head
  (i.e. the tape was already consumed by a retain GRAD), raise an explicit
  error ("second derivatives unsupported") instead of silently returning
  the first derivative → converts silent-wrong-answer into a clean error.
- Candidate for a v0.57.4 hotfix.

- **Related:** TD-046 (PUSH/closures/FFI), TD-047 (MAP value ownership).
- **Target:** v0.59.0+ (differentiable backward or forward-mode epic).

## TD-045: `SET x TO <new tensor>` overwrites old x without freeing — WONTFIX
- **Status:** 🔶 WONTFIX (documented workaround; v0.57.2)
- **Introduced:** pre-v0.57.0c (discovered during TD-041 Phase 2)
- **Impact:** LOW (one tensor leak per SET-overwrite; bounded ~72 B)
- **Symptom:** `SET c TO TENSOR_ADD(...)` on an initialized tensor leaks
  the old value (72 B).
- **WONTFIX rationale (empirical):**
  A-lite (free-before-overwrite + retain-on-alias, SET only) is **not
  sound**. The pre-existing aliasing hazard was confirmed:
  `CREATE d (TENSOR) TO c.` emits `PlantTensor* d = c;` (no retain), and
  `TENSOR_FREE(d); TENSOR_FREE(c);` yields **13 invalid-free errors**.
  Adding free-before-overwrite would turn the leak into a
  **use-after-free** (`d` dangles after `SET c TO …`). The sound fix
  requires the complete ownership model (CREATE + SET + containers +
  PUSH + GIVE + 2 runtime magic-guards) — the v0.58.0 ownership epic.
  Priority: no corruption over no leak (v0.56.0c lesson).
- **Workaround:**
  - Declaration-only initial: `CREATE c (TENSOR) TO TENSOR_ADD(a,b).`
    (no old value to leak).
  - OR: `TENSOR_FREE(c).` before overwriting.
  - Do **not** alias tensor variables (`CREATE d TO c` is broken today).
- **Related:** TD-044, TD-046, TD-047.
- **Future:** Bundled into the ownership/lifetime epic (v0.58.0+).

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
