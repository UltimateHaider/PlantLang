# Runtime Split Plan

## Current State

The PlantLang runtime is a single `plant_runtime.c` file (~8930 lines) containing:
- Core language operations
- String operations
- List/array operations
- Matrix/linear algebra
- CAS (Computer Algebra System)
- PDE solvers
- I/O and FFI
- Test framework

## Split Strategy

### Phase 1: Extract CAS (v0.52.0)

Move `plant_math.c` into a standalone CAS library:
```
runtime/c/
├── plant_runtime.c      (core + I/O + FFI)
├── plant_math.c         (CAS engine)
├── plant_math.h         (CAS API)
├── plant_compat.h       (backward compatibility)
└── plant_report.c       (test reporting)
```

**Rationale**: CAS is self-contained, has its own data structures (MathNode), and can be used independently.

### Phase 2: Extract Matrix Operations (v0.53.0)

Move matrix/linear algebra to a separate file:
```
runtime/c/
├── plant_runtime.c      (core + I/O + FFI)
├── plant_math.c         (CAS engine)
├── plant_matrix.c       (matrix operations)
├── plant_matrix.h       (matrix API)
├── plant_compat.h
└── plant_report.c
```

### Phase 3: Extract String Operations (v0.54.0)

Move string functions to a separate file:
```
runtime/c/
├── plant_runtime.c      (core + I/O + FFI)
├── plant_math.c         (CAS engine)
├── plant_matrix.c       (matrix operations)
├── plant_string.c       (string operations)
├── plant_string.h       (string API)
├── plant_compat.h
└── plant_report.c
```

### Phase 4: Extract List Operations (v0.55.0)

Move list/array functions to a separate file:
```
runtime/c/
├── plant_runtime.c      (core + I/O + FFI)
├── plant_math.c         (CAS engine)
├── plant_matrix.c       (matrix operations)
├── plant_string.c       (string operations)
├── plant_list.c         (list operations)
├── plant_list.h         (list API)
├── plant_compat.h
└── plant_report.c
```

## Benefits

1. **Faster compilation**: Only recompile changed modules
2. **Better organization**: Clear separation of concerns
3. **Easier testing**: Test each module independently
4. **Smaller binaries**: Link only needed modules
5. **Better documentation**: Each module has focused docs

## Risks

1. **Circular dependencies**: Must carefully manage header includes
2. **Global state**: Some globals shared across modules
3. **Build complexity**: Makefile changes required
4. **Self-hosting**: Must verify chain still works after each split

## Migration Strategy

1. Extract one module at a time
2. Verify self-hosting after each extraction
3. Run full test suite after each extraction
4. Update documentation after each extraction
