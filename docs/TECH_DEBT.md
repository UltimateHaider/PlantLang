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

## TD-015: Global/cross-scope lifecycle tracking — PARTIAL CLOSED
- **Status:** PARTIAL CLOSED in v0.51.3b
- **Resolved:**
  - GLOBAL keyword (v0.51.3a).
  - GLOBAL in SUITE/IF/CYCLE (v0.51.3b).
  - GLOBAL_FREE (v0.51.3b).
  - GLOBAL list mutation via `PUT … INTO …` (v0.51.3b) — the only
    supported list mutation (see TD-020 for the unsupported forms).
  - Init order documented (v0.51.3b).
- **Remaining:**
  - Auto-cleanup → v0.51.4.
  - GLOBAL in nested functions → v0.51.4.
  - ACTION-body numeric GLOBAL in `nums` (partial: a numeric global is
    in the top-level `nums`, but not in an ACTION body's `nums`).
  - **Nested-block cross-scope free** (original TD-015 finding): a
    `LIST_FREE` inside an `IF`/loop body of an outer variable is not
    visible to the enclosing scope → a later `FREE` double-frees.
    Cause: `freed_vars` is block-scoped (v0.51.2c.1).

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

## TD-019: Top-level SUITE terminates parsing
- **Status:** OPEN
- **Introduced:** pre-v0.51 (long-standing)
- **Target fix:** v0.51.4
- **Impact:** MEDIUM — statements after `/SUITE.` are dropped.
- **Symptom:**
```plantlang
SUITE "A"
  SHOW "a".
/SUITE.
SHOW "end".   # ← dropped
```
- **Cause:** the top-level SUITE parser is terminal.
- **Discovered by:** v0.51.3b Phase 1 (GLOBAL in SUITE testing).
- **Related:** TD-020 (list ops).

## TD-020: List operations unsupported
- **Status:** OPEN
- **Introduced:** pre-v0.51 (long-standing)
- **Target fix:** v0.51.4 or v0.52.0
- **Impact:** MEDIUM — three common list mutations are not supported:
  - `SET lst TO lst + [item]` (list concat) → invalid C.
  - `SET lst[i] TO value` (index assignment) → not lowered.
  - `PUSH(lst, item)` (push) → `PUSH` is not a keyword.
- **Supported:** `PUT item INTO lst.` (append only).
- **Affects:** locals and globals alike (not GLOBAL-specific).
- **Discovered by:** v0.51.3b Phase 3.
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

## TD-022: DBL display truncates doubles
- **Status:** OPEN
- **Introduced:** v0.51.4 (SCL made numeric); renamed DBL in v0.51.10.
- **Target fix:** v0.51.5 or v0.52.0
- **Impact:** LOW-MEDIUM — `SHOW D + 1.` for DBL truncates (`4.14` → `4`).
- **Cause:** `_from_long` wraps a double → integer truncation.
- **Fix direction:** use `_from_double` for doubles, or type-aware wrapping.
- **Discovered by:** v0.51.4 Phase 4.
- **Related:** TD-021.

## TD-024: DCM (long double) size is implementation-defined — OPEN
- **Status:** OPEN
- **Introduced:** v0.51.10 (DCM added).
- **Target fix:** v0.52.0 (Type System Audit) or document.
- **Impact:** LOW — DCM behavior varies by platform.
- **Issue:** In C89, `long double` size/precision is
  implementation-defined:
  - x86: 80-bit extended precision.
  - ARM: may be 64-bit (same as double) or 128-bit.
- **User guidance:** Avoid relying on DCM having more precision
  than DBL across platforms.
- **Related:** TD-022.

## TD-023: UNUM (unsigned long) removed — CLOSED in v0.51.9
- **Status:** ✅ CLOSED in v0.51.9
- **Reason:** Duplicated by ULO.
- **Removed:** UNUM record + any references.
- **Replacement:** ULO (unsigned long).
- **Related:** TD-022.

## TD-025: CMP arithmetic is target-typed — OPEN
- **Status:** OPEN
- **Introduced:** v0.51.11 (CMP added).
- **Target fix:** v0.52.0 (Type System Audit).
- **Impact:** LOW — `CREATE S (CMP) TO A + B.` works (target type
  drives lowering), but operands in untargeted expression positions
  (e.g. `SHOW CABS(A + B).`) are not lowered to complex ops.
- **Cause:** CMP lowering is keyed off the CREATE/LET target type,
  not full operand type inference.
- **Fix direction:** track CMP variables like `nums` and lower ops on
  operand types.
- **Related:** TD-022, TD-024.

## TD-026: TX (text alias) deprecated — remove in v0.52.0
- **Status:** DEPRECATED in v0.51.13
- **Target removal:** v0.52.0
- **Impact:** LOW — TX still works; no behavior change.
- **Reason:** TX is the legacy 2-letter text name; TXT is canonical.
- **Action:** Migrate code to TXT; remove TX in v0.52.0.
- **Related:** TD-018.
