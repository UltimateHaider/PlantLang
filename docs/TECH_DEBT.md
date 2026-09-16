# Technical Debt Register — PlantLang Chloroplast

## TD-001: PlantArray never freed
- **Status**: OPEN
- **Introduced**: v0.0.1 (always existed)
- **Target fix**: v0.51.2
- **Impact**: 48–168 bytes per PlantArray (capacity-dependent)
- **Root cause**: `plant_list_free()` does not exist in the runtime
- **Fix plan**: Add `plant_list_free(PlantArray* a)` to `plant_runtime.c` that frees `a->items` then `a`. Update codegen to emit cleanup calls at scope end.
- **Scope**: Runtime + codegen
- **valgrind evidence**: Compiled `lists.plant` → 40 bytes lost in `plant_list_make`

## TD-002: PlantTensor never freed
- **Status**: OPEN
- **Introduced**: v0.51.1
- **Target fix**: v0.51.7
- **Impact**: 88–128 bytes per tensor (dimensionality-dependent)
- **Root cause**: Generated code never calls `plant_tensor_free()`. The function exists in `plant_tensor.c` but is not declared in `plant_compat.h` (now added) and codegen does not emit calls.
- **Fix plan**: Codegen emits `plant_tensor_free(T)` at scope end for each `CREATE T TO TENSOR(...)` statement.
- **Scope**: Codegen only
- **valgrind evidence**: tensor_refcount → 7 PlantTensor leaks (704 bytes)

## TD-003: to_string buffer never freed
- **Status**: OPEN
- **Introduced**: v0.51.1
- **Target fix**: v0.51.2+
- **Impact**: 1,024 bytes per SHOW TENSOR statement
- **Root cause**: `plant_tensor_to_string()` returns a heap-allocated string. Generated code passes it directly to `plant_iReport_print()` without calling `free()`.
- **Fix plan**: Codegen emits `char* _s = plant_tensor_to_string(T); plant_iReport_print(r, _s); free(_s);` pattern.
- **Scope**: Codegen only
- **valgrind evidence**: tensor_refcount → 1,024 bytes from `_tensor_to_string_recursive`

## TD-004: valgrind-check was measuring compiler, not runtime
- **Status**: FIXED (v0.51.1)
- **Introduced**: v0.51.0b
- **Fixed**: v0.51.1
- **Impact**: Compiler leaks (87KB+) masked runtime leaks (1–2KB)
- **Root cause**: Original `valgrind-check` ran `valgrind bin/Chloroplast tests/native/$$test.plant` — valgrinding the compiler process, not the compiled test binary.
- **Fix**: Target now compiles test → gcc → valgrind on compiled binary.
- **Scope**: Makefile only
