# Development Principles (v0.51.2a+)

Permanent principles for all future releases v0.51.x → v0.56.x.

## Principles

### A. plant_malloc Exclusively
ALL allocations in new files MUST use `plant_malloc()` (from v0.51.0b), NEVER raw `malloc()`. Gated by `PLANT_MALLOC_SIMULATE` for test injection.

### B. Recursion Depth Increases by 1 Every Release
| Version | Max Dimension |
|---------|---------------|
| v0.51.1 | 5D |
| v0.51.2 | 6D |
| v0.51.3 | 7D |
| v0.51.4 | 8D |
| v0.51.5 | 9D |
| v0.51.6 | 10D |
| v0.51.7 | 11D |

### C. Parser Regression with 4-Level Nesting
Every release MUST test 4-level nesting: TENSOR → LIST → MATRIX → expression.

### D. ref_count + Chained deep_copy (5 Levels)
Every release MUST test ref_count and deep_copy at 5 levels.

### D2. ref_count + View (Future)
Planned for v0.51.7+: view semantics with shared data.

### E. Size Report with Diff + Chart
REAL numbers (no placeholders). ASCII chart with cumulative growth.

### E2. SVG/JSON Charts (Future)
Planned for v0.52.0+.

### F. STOP on Memory Failure or 10% Growth
STOP immediately on:
- Memory failure (malloc returns NULL in production)
- Binary size increase >10% in TWO consecutive releases

### F2. STOP on >5% Test Failure Rate
STOP immediately if more than 5% of tests fail in any build.

### G. Automated Grep Check for Every Malloc
`make check-no-raw-malloc` verifies no raw `malloc()` in new files.

### H. CHANGELOG with Cumulative Growth Table
Every release MUST include a cumulative growth table with REAL numbers.

### I. Tests Must Produce Actual Output
No placeholder comments in .expected files. Every test must have real output.

### J. valgrind Mandatory on deep_copy/ref_count Tests
valgrind MUST run on memory-critical tests (ref_count, malloc, deep_copy).
- **v0.51.1**: report-only mode. Pre-existing leaks (TD-001/002/003) documented and grandfathered.
- **v0.51.2+**: strict mode required. `--error-exitcode=1` for NEW tensor-specific leaks.
- **Rule**: Any NEW leak in `plant_tensor.c` or `plant_tensor.h` MUST be fixed before release. No grandfathering for new code.

### K. verify-v0.51.1 Target
Comprehensive release gate: runs all checks (C tests, native tests, valgrind, self-hosting, size report, binary growth check).

### L. Memory Management Built-ins (v0.51.2a)
Codegen provides `LIST_FREE`, `TENSOR_FREE`, and `FREE` built-ins for explicit memory management. TD-001/002/003 closed.

### M. plant_raw_free for Internal Use
`plant_raw_free()` is the simple `free()` wrapper for internal runtime use. `plant_free()` is the generic type-aware dispatcher.

### N. Compile-time Magic Verification
`ASSERT_MAGIC_FIRST(type)` ensures `magic` is always the first field in PlantArray and PlantTensor structs.

### P. _PLANT_PTR_MIN Guard
All type-detection code checks `(uintptr_t)ptr >= 0x1000` before dereferencing, preventing segfaults on small integer values cast to void*.

### T. SHOW Tensor Emits free(__str)
`gen_show_stmt` wraps tensor SHOW output in `{ char* __str = ...; plant_iReport_print(r, __str); free(__str); }` to close the to_string leak.

### W. FREE Generic Dispatcher (Closed v0.51.2b)
`plant_free()` is the generic type-aware dispatcher. Since v0.51.2b, codegen emits `plant_free()` for all three built-ins (LIST_FREE, TENSOR_FREE, FREE). No type-specific calls remain in generated code.

### EE. TENSOR Introspection Return Types (v0.51.2b)
`TENSOR_SHAPE` returns `PlantArray*` with string-ified shape values. `TENSOR_NDIM` and `TENSOR_SIZE` return `int64_t`. Shape values are stored as `strdup(snprintf(...))` strings, not raw integer casts.

### FF. plant_free Returns Void (v0.51.2b)
`plant_free()` returns `void`. Codegen must NOT assign its result (e.g., `x = plant_free(x)` is invalid; use `plant_free(x);` instead).

### GG. FREE Keyword vs Function (v0.51.2b)
`FREE` is a keyword with syntax `FREE x.` (no parentheses). `LIST_FREE(x)` and `TENSOR_FREE(x)` are regular function calls with parentheses.

### HH. Expected Files Trailing Newline (v0.51.2b)
All `.expected` files must end with a trailing newline. The test runner's diff comparison fails if the file lacks a final newline.

### II. seg_is_numeric Prefix Length (v0.51.2b)
`seg_is_numeric` prefix strings must match exactly (e.g., `plant_tensor_ndim(` = 18 chars, not 17). Off-by-one causes introspection built-ins to be treated as non-numeric.

### Y. valgrind-check-tensor Strict Mode
`valgrind-check-tensor` uses `--error-exitcode=1` and `--errors-for-leak-kinds=definite` to catch any new leaks.

### AA. check-ffi-safety
FFI examples must not use `plant_tensor_to_string_static` (internal buffer API).

### BB. check-releases-updated
RELEASES.md must be updated with current version before release.

### CC. check-verify-integrity
All verify-* targets must include valgrind-check-tensor.

### DD. update-growth Script
`scripts/update_growth_table.sh` automates growth table updates in RELEASES.md.

### TT. Variable lifecycle tracking (v0.51.2c.1+)

    - Codegen tracks explicitly-freed variables in env slot 13
      (`freed_vars`).
    - Auto-cleanup skips variables present in `freed_vars`.
    - Block-scoped via `generate_body` (snapshot/restore).
    - Scope: local variables, loops, if, mixed types.
    - Out of scope: nested functions, struct fields, global,
      closure, static, nested structs.

### YY. Global/Closure/Static fields out of scope (v0.51.2c.2+)

    - Fields of global/closure/static variables are NOT tracked.
    - Target: v0.51.2d.

### YY2. Struct support is broken (v0.51.2c.2+)

    - Structs are fundamentally broken in the current compiler.
    - Cannot be used reliably in production.
    - See docs/TECH_DEBT.md (TD-009).
    - Target fix: v0.52.0 (Type System Audit).

### GGG. Runtime change exception (v0.51.2d.1)

    - TD-010 required a runtime change in plant_memory.c,
      plant_compat.h, and plant_runtime.c (one line).
    - This was explicitly authorized and documented.
    - Future runtime changes must be similarly justified.

### HHH. Header-tagged heap strings (v0.51.2d.1)

    - Numeric conversions (_from_long >= 10, _from_double) allocate
      strings with an 8-byte magic header.
    - plant_heapstr_release checks the magic before freeing.
    - Safety: no false free for static literals (magic mismatch).
    - Cost: one bounded read before the string pointer.

### JJ. Back-compat tests required (v0.51.2d.2+)

    - Every codegen change MUST include a back-compat test.
    - Tests must exercise BOTH old API names and new ones.
    - For v0.51.2d.2: backcompat tests cover:
      - LIST_FREE / TENSOR_FREE / FREE aliases.
      - Old code patterns (v0.51.2a style).
      - Mixed patterns.
      - Edge cases.
    - Rationale: API changes must preserve backward compatibility
      without silent breakage.
    - Status: MANDATORY from v0.51.2d.2 onward.

### KKK. Global tracking deferred (v0.51.2d.2)

    - Global variables in PlantLang are main() locals; not visible
      to ACTIONs. No cross-function mutable state exists.
    - Global lifecycle tracking is therefore not applicable in the
      current architecture.
    - Cross-scope (nested-block) free tracking is deferred to
      v0.52.0 (Type System Audit) — see TD-015.

### LLL. Static variables deferred (v0.51.2d.2)

    - STATIC is not a keyword in PlantLang.
    - STATIC declarations are deferred to v0.52.0 (TD-016).

### MMM. Closure capture lifecycle deferred (v0.51.2d.2)

    - Closure captures use heap env structs (plant_env_alloc).
    - Env lifecycle is not individually managed.
    - Deferred to v0.52.0 (TD-017).

### QQQ. GLOBAL keyword (v0.51.3a)

    - GLOBAL declares a mutable variable at file scope.
    - Syntax: GLOBAL name TO value. / GLOBAL name (TYPE) TO value.
    - Visibility: all ACTIONs.
    - Lifecycle: persistent (no auto-cleanup).
    - Storage: C `static` at file scope (C89-compatible:
      declaration without init + main-position assignment).
    - Partially resolves TD-015.

### RRR. GLOBAL scope (v0.51.3a)

    - GLOBAL only at top-level (module scope).
    - GLOBAL inside SUITE/IF/loop bodies is NOT processed.
    - Nested GLOBAL support deferred to v0.51.4.

### SSS. GLOBAL lifecycle (v0.51.3a)

    - GLOBAL variables are persistent.
    - No auto-cleanup at program exit.
    - GLOBAL_FREE deferred to v0.51.4.

### TTT. GLOBAL scope-aware (v0.51.3b)

    - GLOBAL can be declared inside SUITE/IF/CYCLE bodies.
    - It is treated as a top-level declaration (once).
    - The init runs at the statement's position.

### UUU. GLOBAL_FREE (v0.51.3b)

    - `GLOBAL_FREE(name).` frees a GLOBAL explicitly.
    - Re-assignment after GLOBAL_FREE is allowed.
    - No freed_vars tracking (GLOBAL persistent).

### VVV. GLOBAL init order (v0.51.3b)

    - Declaration order.
    - Forward references NOT supported.
    - No compile-time check.

### WWW. GLOBAL persistent (v0.51.3b)

    - GLOBAL variables are persistent (programme-scope).
    - No auto-cleanup at program exit.
    - Auto-cleanup deferred to v0.51.4.

### XXX. Type Registry (v0.51.4)

    - Single source of truth for scalar types (LON, NUM, ULO,
      UNU, SCL, CHAR, BYTES).
    - Stored as GLOBAL TYPE_REGISTRY in codegen_c.plant.
    - Records: [name, ctype, numeric, prim, category, default].
    - Adding a scalar type = one record (not 6-8 edits).

### YYY. Accessor pattern (v0.51.4)

    - type_info(name) → record or empty.
    - type_ctype(name) → C type string; fallback "tx_t".
    - type_is_numeric(name) → "1" or "0".
    - type_is_prim(name) → "1" or "0".
    - All return strings (consistent with codegen conventions).

### ZZZ. Migration byte-identical (v0.51.4)

    - Migration from IF-chains to registry must NOT change generated C
      for any existing scalar type (verified for is_numeric_type,
      collect_nums*, plant_ctype).

### XXX2. is_prim_type not migrated (v0.51.4)

    - is_prim_type retains its IF-chain.
    - Reason: callers are closure-scope; registry prim semantics
      don't match the current table (LON/NUM/BYTES only).
    - The registry remains the source of truth for type_ctype,
      type_is_numeric (and future type_is_prim if the semantics
      are aligned).
    - Deferred: aligning registry prim with is_prim_type semantics
      → v0.51.5 or v0.52.0.

### AAA. `IS` is type-aware (v0.51.5)

    - `IS` works for all operand combinations:
      - literals vs literals (existing; preserved).
      - variables vs variables (NEW).
      - runtime-built strings (NEW).
      - numeric variables (via nums).
    - Type-aware: numeric operands → `==`; string operands → strcmp.
    - NULL/TRUE/FALSE excluded (compared by value, not string identity).
    - Behavior is consistent across scopes (top-level, ACTION, IF,
      CYCLE).
    - C89-compatible generated C.

### BBB. `IS` consistency (v0.51.5)

    - Literal-vs-literal and variable-vs-variable behave identically.
    - Runtime-built strings (concatenation) are compared by value.
    - Regression tests guard against future breakage:
      is_variables_string, is_variables_num, is_in_action,
      is_dynamic_string.
