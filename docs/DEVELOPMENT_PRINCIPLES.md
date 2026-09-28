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
