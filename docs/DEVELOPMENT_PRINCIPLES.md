# Development Principles (v0.51.1+)

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
