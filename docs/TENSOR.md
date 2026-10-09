# PlantTensor Type (v0.51.1)

## Overview

PlantTensor is a NEW native type for N-dimensional numerical tensors, introduced in v0.51.1. It is NOT an extension of PlantArray — it has contiguous `double*` data storage, row-major memory layout, and reference-counted lifecycle.

## Type Definition

```c
typedef struct PlantTensor {
    uint64_t magic;      /* PLANT_TENSOR_MAGIC */
    int64_t  ndim;       /* Number of dimensions (1..N) */
    int64_t* shape;      /* shape[0..ndim-1] */
    int64_t* strides;    /* Row-major strides, in elements */
    double*  data;       /* Contiguous row-major storage */
    int64_t  size;       /* Total elements (product of shape) */
    int64_t  ref_count;  /* Reference count (starts at 1) */
    char*    error_msg;  /* NULL if no error */
} PlantTensor;
```

## Construction

### 1D Tensor
```plantlang
SHOW TENSOR([1, 2, 3]).        # [1, 2, 3]
```

### 2D Tensor
```plantlang
SHOW TENSOR([[1, 2], [3, 4]]). # [[1, 2], [3, 4]]
```

### 3D Tensor
```plantlang
SHOW TENSOR([[[1, 2]]]).       # [[[1, 2]]]
```

### 4D and 5D
```plantlang
SHOW TENSOR([[[[1, 2]]]]).     # [[[[1, 2]]]]
SHOW TENSOR([[[[[1, 2]]]]]).   # [[[[[1, 2]]]]]
```

## Display

Tensors display in nested-list format:
- 1D: `[1, 2, 3]`
- 2D: `[[1, 2], [3, 4]]`
- 3D: `[[[1, 2], [3, 4]], [[5, 6], [7, 8]]]`

## Memory Model

- **ref_count**: Starts at 1. Decremented on free. Tensor freed when ref_count reaches 0.
- **deep_copy**: Creates independent clone with ref_count=1.
- **row-major**: Element [i, j, k] = data[i*strides[0] + j*strides[1] + k*strides[2]].

## Known Memory Leaks (v0.51.1)

All leaks are **bounded** (no accumulation in loops) and **freed at process exit**.

### Per-tensor leak breakdown

| Component | Bytes | Freed? | Fix target |
|-----------|-------|--------|------------|
| PlantArray (from `plant_list_make`) | 48–168 | YES (v0.51.2a) | TD-001 CLOSED |
| PlantTensor struct + fields | 88–128 | YES (v0.51.2a) | TD-002 CLOSED |
| `plant_tensor_to_string` buffer | 1,024 | YES (v0.51.2a) | TD-003 CLOSED |

### Loop behavior (mitigating)

PlantLang loops optimize variable reuse:
```plantlang
LOOP i FROM 1 TO 100:
  CREATE T TO TENSOR([1, 2, 3]).
/LOOP
```
Result: Only 1 tensor exists at exit (not 100). The compiler reuses the variable slot. **Leaks do NOT accumulate.**

### LST OF TENSOR warning (non-mitigating)

Lists of tensors WILL accumulate leaks:
```plantlang
CREATE L TO LST OF TENSOR(...).
```
Each new tensor in the list leaks ~240 bytes. This is a known risk for future ML workloads. Fix requires codegen cleanup (TD-002, now closed).

## Memory Management (v0.51.2a+)

### Built-ins (All route through plant_free dispatcher since v0.51.2b)
- `LIST_FREE(x)` — Recursively frees a PlantArray and its contents
- `TENSOR_FREE(x)` — Frees a PlantTensor and its data
- `FREE x.` — Generic type-aware dispatcher (same as above)

### Aliases
`LIST_FREE`, `TENSOR_FREE`, and `FREE` are functionally identical in v0.51.2b+. All three route through the `plant_free()` generic dispatcher. Use whichever reads best in context:
- `LIST_FREE(x)` — when x is known to be a list
- `TENSOR_FREE(x)` — when x is known to be a tensor
- `FREE x.` — when the type is unknown or mixed

### Example
```plantlang
ACTION main(),
  LET mylist TO [1, 2, 3].
  LET mytensor TO TENSOR([1, 2, 3, 4, 5, 6]).
  SHOW mylist.
  SHOW mytensor.
  LIST_FREE(mylist).
  TENSOR_FREE(mytensor).
/ACTION.
```

## Memory Tracking Scope (v0.51.2c.2)

Covered:
- ✅ Local variables
- ✅ Loops and if branches
- ✅ Nested functions (per-function freed_vars)

Not covered:
- ❌ Struct fields (structs are fundamentally broken — TD-009)
- ❌ Multi-level struct access (h.a.b)
- ❌ Heap-allocated structs
- ❌ Global struct fields
- ❌ Closure struct fields
- ❌ Static variables inside struct
- ❌ Nested structs

See docs/TECH_DEBT.md (TD-006, TD-008, TD-009).

## What's NOT Supported Yet

- Operations (add, multiply, reshape, transpose) — deferred to v0.51.2+
- GPU acceleration — future phase
- Sparse tensors — future phase
- Broadcasting — v0.51.6

## Introspection Built-ins (v0.51.2b)

| Built-in | Return Type | Description |
|----------|-------------|-------------|
| `TENSOR_SHAPE(x)` | `PlantArray*` (list of integers) | Shape values via `_from_long`, e.g. `[2, 3]` |
| `TENSOR_NDIM(x)` | `int64_t` | Number of dimensions |
| `TENSOR_SIZE(x)` | `int64_t` | Total element count (product of shape) |

### TENSOR_SHAPE Return Type

Returns a **List** of shape values. Values are stored as PlantLang integers (via `_from_long`), which are internally `char*` strings — consistent with all PlantLang integers.

```plantlang
CREATE T TO TENSOR([[1, 2], [3, 4]]).
CREATE S TO TENSOR_SHAPE(T).
SHOW S.           # [2, 2]
TENSOR_FREE(T).
```

## Roadmap

| Version | Feature |
|---------|---------|
| v0.51.1 | TENSOR Core (this release) |
| v0.51.2a | Core Memory Fixes (TD-001..004) |
| v0.51.2b | Introspection + Interface (TENSOR_SHAPE/NDIM/SIZE, TD-005) |
| v0.51.3 | TENSOR Reshape |
| v0.51.4 | TENSOR Transpose |
| v0.51.5 | TENSOR Operations |
| v0.51.6 | TENSOR Broadcasting |
| v0.51.7 | TENSOR Views |

## Relationship with MATH and Matrix

- **MATH**: Symbolic math engine (plant_math.c). PlantTensor is for numerical computation.
- **Matrix**: PlantArray-based nested lists with MAT_ADD, MAT_SUB, etc. PlantTensor is a separate type with contiguous storage.
- Future: TENSOR operations will replace Matrix operations for numerical work.
