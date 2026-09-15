# Tensor Operations Roadmap

## Current State (v0.51.0b)

PlantLang supports 2D matrices as nested lists with:
- Element-wise operations: MAT_ADD, MAT_SUB
- Structural: MAT_TRACE, MAT_IDENTITY
- Linear algebra: MATRIX_MULT, TRANSPOSE, DET, INVERSE, LU, EIGEN, SVD
- Vector: DOT, CROSS, NORM
- PDE solvers: Wave, Heat, Laplace

All operations are limited to 2D matrices (up to 2000×2000).

## Phase 1: Broadcast Semantics (v0.52.0)

Add automatic broadcasting for scalar-matrix and matrix-vector operations:
- `scalar + matrix` → element-wise addition
- `matrix * vector` → matrix-vector product
- `matrix + matrix` → element-wise with broadcast rules

## Phase 2: Higher-Order Tensors (v0.53.0)

Extend the nested-list model to support 3D+ tensors:
- `TENSOR_CREATE(shape, values)` — create tensor with explicit shape
- `TENSOR_RESHAPE(tensor, new_shape)` — reshape without copy
- `TENSOR_TRANSPOSE(tensor, axes)` — generalized transpose
- `TENSOR_MATMUL(A, B, axes)` — batch matrix multiplication

## Phase 3: Differentiable Operations (v0.54.0)

Add automatic differentiation support for tensor operations:
- `TENSOR_GRAD(f, vars)` — compute gradient tensor
- `TENSOR_JACOBIAN(f, vars)` — compute Jacobian matrix
- Integration with existing MATH_DERIVATIVE

## Phase 4: Sparse Tensors (v0.55.0)

Memory-efficient representation for sparse data:
- `SPARSE_CREATE(indices, values, shape)` — COO format
- `SPARSE_MATMUL(A, B)` — sparse matrix multiplication
- Automatic dense/sparse selection based on sparsity

## Design Principles

1. **Nested lists first**: Higher-order tensors build on existing PlantArray
2. **No new token types**: All tensor ops use existing function call syntax
3. **Lazy evaluation**: Tensor operations return deferred computations
4. **Memory safety**: All allocations use heap with automatic cleanup
5. **Self-hosting**: All new features must compile through the self-hosting chain
