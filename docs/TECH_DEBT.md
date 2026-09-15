# Technical Debt Tracker

## Active Items

### Runtime Monolith (plant_runtime.c)
- **File**: `runtime/c/plant_runtime.c` (~8930 lines)
- **Issue**: Core language, matrix ops, CAS, PDE solvers, string ops, list ops, I/O, FFI, and test framework all in one file
- **Impact**: Slow incremental compilation, hard to navigate, difficult to test independently
- **Mitigation**: Runtime split plan documented in `RUNTIME_SPLIT_PLAN.md`
- **Target**: Phase 1 (extract CAS) in v0.52.0

### Matrix Allocation Path (malloc simulation scope)
- **Files**: `_la_alloc`, `_la_alloc_rect` in `plant_runtime.c`
- **Issue**: Malloc failure simulation only covers matrix allocation path (4 malloc calls)
- **Impact**: Other allocation failures (string ops, list creation, CAS) cannot be simulated in tests
- **Mitigation**: Sufficient for current test coverage; extend in future if needed
- **Target**: v0.52.0+ if broader allocation testing is needed

### Self-Hosting Binary Size
- **Current**: 891,624 bytes (v0.51.0b, v3 == final)
- **Threshold**: WARN at 10% growth (>980,786), CRITICAL at 20% (>1,069,949)
- **Impact**: Large binary affects distribution and startup time
- **Mitigation**: Size-report target tracks growth; runtime split in v0.52.0 may help
- **Target**: Ongoing monitoring

### Error Message Consistency
- **Status**: Unified in v0.51.0 for matrix operations (10 error messages)
- **Issue**: Other subsystems (CAS, PDE, string ops) still have ad-hoc error formatting
- **Impact**: Inconsistent error messages across subsystems
- **Mitigation**: None currently
- **Target**: v0.52.0+ systematic error message audit

### PDE Solver Coverage
- **Status**: Wave, Heat, Laplace supported
- **Issue**: Only 3 PDE types; no support for Schrödinger, Maxwell, Navier-Stokes, etc.
- **Impact**: Limited applicability for physics/engineering use cases
- **Mitigation**: Documented in `PDE_LIMITATIONS.md`
- **Target**: v0.52.0+ expansion

## Resolved Items

### malloc simulation scope (v0.51.0b)
- **Issue**: Tests could not verify allocation failure behavior
- **Resolution**: Added `PLANT_MALLOC_SIMULATE` compile flag and `MAT_MALLOC_FAIL_AT` env var
- **Scope**: Matrix allocation path only (`_la_alloc`, `_la_alloc_rect`)
- **Date**: 2026-09-15
