# Testing Strategy

## Test Pyramid

```
                    ┌─────────────┐
                    │   E2E Tests │  (Full program compilation + execution)
                   ┌┴─────────────┴┐
                   │ Integration    │  (Runtime + codegen interaction)
                  ┌┴───────────────┴┐
                  │   Unit Tests    │  (Individual function testing)
                 ┌┴─────────────────┴┐
                 │   Static Analysis │  (Type checking, linting)
                └───────────────────┘
```

## Test Categories

### 1. Unit Tests (C level)

**Location**: `runtime/c/` (inline `#ifdef TEST` blocks)

**Coverage**:
- Matrix operations: MAT_ADD, MAT_SUB, MAT_TRACE, MAT_IDENTITY
- CAS functions: derivatives, integrals, simplification
- PDE solvers: Wave, Heat, Laplace verification
- String operations: concatenation, parsing, formatting
- List operations: creation, access, modification

**Example**:
```c
#ifdef TEST
void test_mat_add() {
    PlantArray* a = plant_list_make(2, 1.0, 2.0);
    PlantArray* b = plant_list_make(2, 3.0, 4.0);
    PlantArray* result = plant_mat_add(a, b);
    assert(result->values[0] == 4.0);
    assert(result->values[1] == 6.0);
}
#endif
```

### 2. Native Tests

**Location**: `tests/native/`

**Coverage**: Complete language features through compilation and execution.

**Test Count**: 30 tests (as of v0.51.0b)

**Categories**:
- CLI tests (3): help, version, error handling
- Language features (17): arrays, strings, math, control flow, PDE, matrices
- Standard library (7): queues, sets, stacks, JSON
- Matrix stress (3): mat_basic, mat_err, mat_malloc

**Special: Malloc Failure Tests**:
`mat_malloc.plant` is compiled with `-DPLANT_MALLOC_SIMULATE` and run three times with different `MAT_MALLOC_FAIL_AT` values:
1. `MAT_MALLOC_FAIL_AT=1` — complete allocation failure → `ERR`
2. `MAT_MALLOC_FAIL_AT=3` — partial allocation failure → `ERR`
3. No env var — normal execution → correct result

**Run Command**:
```bash
bash tests/native/run_native_tests.sh
```

### 3. Regression Tests

**Location**: `tests/regression/`

**Coverage**: Individual language features with expected output comparison.

**Test Count**: ~400 tests (as of v0.51.0)

**Categories**:
- Matrix operations (1): matrix.plant
- CAS operations (~50): deriv_*, integr_*, simpl_*, etc.
- Control flow (~30): if_else, match, enum, etc.
- String operations (~20): string_*, interp_*, etc.
- List operations (~30): list_*, concat, etc.
- FFI tests (~10): ffi_*, json_*, etc.

**Run Command**:
```bash
bash tests/regression/run_regression_tests.sh
```

### 4. Generics Tests

**Location**: `tests/generics/`

**Coverage**: Generic type system and polymorphism.

**Run Command**:
```bash
bash tests/generics/run_generics_tests.sh
```

### 5. Closures Tests

**Location**: `tests/closures/`

**Coverage**: Closures, lexical scoping, and capture semantics.

**Run Command**:
```bash
bash tests/closures/run_closures_tests.sh
```

### 6. Stress Tests

**Location**: `tests/regression/stress/`

**Coverage**: Large inputs, memory pressure, performance.

**Run Command**:
```bash
bash tests/regression/run_regression_tests.sh --stress
```

### 7. Smoke Tests

**Location**: `tests/smoke/`

**Coverage**: Rapid validation of core language features.

**Run Command**:
```bash
make smoke
```

## Test Automation

### Pre-commit Hook

Runs native tests and key regression tests before each commit:
```bash
#!/bin/bash
make clean && make all 2>&1 | tail -3
bash tests/native/run_native_tests.sh
if [ $? -ne 0 ]; then
    echo "Tests failed, commit aborted"
    exit 1
fi
```

### CI/CD Pipeline

```yaml
test:
  stages:
    - build
    - unit
    - integration
    - e2e
  scripts:
    - make clean && make all
    - make test
    - make size-report
    - make test-perf
```

## Coverage Metrics

### Current Coverage (v0.51.0b)

| Category | Tests | Coverage |
|----------|-------|----------|
| Native | 30 | 100% |
| Regression | ~400 | 95% |
| Generics | ~50 | 90% |
| Closures | ~30 | 85% |
| Stress | ~20 | 80% |
| **Total** | **~530** | **~92%** |

### Coverage Goals

- **v0.52.0**: 95% regression coverage
- **v0.53.0**: 98% regression coverage
- **v0.54.0**: 100% regression coverage

## Performance Testing

### Metrics

1. **Compilation time**: Time to compile PlantLang to C
2. **Build time**: Time to compile C to binary
3. **Execution time**: Time to run compiled programs
4. **Memory usage**: Peak memory during compilation
5. **Binary size**: Size of final compiled binary

### Benchmarks

```bash
make benchmark    # Full benchmark suite
make test-perf    # Test suite timing
make size-report  # Binary size tracking
```

## Self-Hosting Convergence Criteria

### Historical Note
Early directives assumed "byte-identical v2=v3=v4=v5" would be the
convergence criterion. In practice, a normalization step exists:

  @@CLOSURE@@ → @CLOSURE@

This causes v2.c and v3.c to differ by a few bytes, producing
non-identical (but semantically equivalent) binaries.

### Current Official Criterion (v0.51.0a+)

Convergence is achieved when:
  v3 == final

Specifically:
  - `cmp -s build/plantc_v2 build/plantc_v3` → MAY DIFFER
  - `cmp -s build/plantc_v3 bin/Chloroplast` → MUST BE IDENTICAL
  - MD5 of v3 == MD5 of final

### Verification Commands

```bash
make self
cmp -s build/plantc_v2 build/plantc_v3      # may differ (normalization)
cmp -s build/plantc_v3 bin/Chloroplast      # must be identical
md5sum build/plantc_v3 bin/Chloroplast      # must match
```

### Rationale

The `@@CLOSURE@@` → `@CLOSURE@` normalization is a known and expected
step of the self-hosting pipeline. It ensures deterministic output
from v3 onward. v2 is an intermediate stage, not the final form.

### Historical Verification (v0.51.0)

| Artifact     | Size      | MD5                                    |
|-------------|-----------|----------------------------------------|
| v2 binary   | 891,624   | ca55057257116d2d4928361e068dda13       |
| v3 binary   | 891,624   | 3d54893af95edda2a83d39f04fa33911       |
| final binary| 891,624   | 3d54893af95edda2a83d39f04fa33911       |

v2 ≠ v3 (normalization), v3 == final ✅

---

## Quality Gates

### Pre-release Checklist

- [ ] All native tests pass (30/30)
- [ ] All regression tests pass (~400/400)
- [ ] Self-hosting chain converges
- [ ] No memory leaks (valgrind clean)
- [ ] Binary size within 10% of previous version
- [ ] Documentation updated
- [ ] CHANGELOG updated
- [ ] Version bumped in all files

### Code Review Checklist

- [ ] Tests added for new features
- [ ] Tests pass locally
- [ ] No regressions in existing tests
- [ ] Performance benchmarks run
- [ ] Documentation updated
- [ ] Self-hosting verified
