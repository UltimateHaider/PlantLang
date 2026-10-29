# Tensor Utilities Recipe (v0.62.1)

## Axis reductions (TD-068) — keepdims = false

```plant
CREATE x (TENSOR) TO TENSOR([[1.0, 2.0], [3.0, 4.0]]).
CREATE m (TENSOR) TO MEAN_AXIS(x, 0).    # [2, 3]
CREATE s (TENSOR) TO SUM_AXIS(x, 0).     # [4, 6]
CREATE v (TENSOR) TO VAR_AXIS(x, 0).     # [1, 1]  (biased, ÷M)
```

The reduced axis is removed from the shape (`[2,2]` → `[2]`).
Works for any ndim (`axis ∈ [0, ndim)`).

Backward (`GRAD`) is automatic:
`d sum = d` broadcast, `d mean = d/red`, `d var = 2(x−μ)/red`.

## `sqrt` (elementwise)

```plant
CREATE y (TENSOR) TO TENSOR_SQRT(x).     # x < 0 → error
```

Backward: `1/(2√x)`. Example: `sqrt([4,9,16]) = [2,3,4]`, and
`d/dx Σsqrt(x)² = [1,1,1]`.

## `log` / `exp` / `abs` (TD-069)

```plant
CREATE l (TENSOR) TO TENSOR_LOG(x).      # x ≤ 0 → error
CREATE e (TENSOR) TO TENSOR_EXP(x).
CREATE a (TENSOR) TO TENSOR_ABS(x).
```

Backward: `1/x`, `exp(x)`, `sign(x)`.

## Cross-entropy (composed — no helper needed)

```plant
CREATE logits (TENSOR) TO TENSOR([[1.0, 2.0]]).
CREATE T      (TENSOR) TO TENSOR([[0.0, 1.0]]).
CREATE p   (TENSOR) TO TENSOR_SOFTMAX(logits, 1).
CREATE lp  (TENSOR) TO TENSOR_LOG(p).
CREATE tl  (TENSOR) TO TENSOR_MUL(T, lp).
CREATE loss (DBL)   TO TENSOR_SUM(tl).
SHOW loss.                               # -0.3132616875
# minimise the negative (or use -loss) to train
```

## TD-072: `_to_long_release`

`-> LON` ACTION returns are now **released** at the caller coercion site, so
multi-digit values no longer leak:

```plant
ACTION f() -> LON
GIVE 42.
/ACTION.
CREATE v (LON) TO f().   # 0 B (was 11 B)
```

User variables keep plain `_to_long` (no double-free risk). Temporaries are
magic-checked, so static digit/double strings are safe no-ops.

## Limitations

- **TD-071** (MED, silent-wrong): an explicit `-> LON` ACTION call **inside an
  expression** (`inner() + 1`) is still not coerced → pointer arithmetic.
  Workaround: bind first —
  `CREATE t (LON) TO inner().` / `SET t TO t + 1.` — then use `t`.
  (The TD-071 pattern also still leaks its returned string.)
- **ADv2** (`GRAD_HO`) is unsupported for all 7 new ops — explicit error, e.g.
  `ad2: MEAN_AXIS unsupported (use GRAD)`.
- `log`/`sqrt` reject invalid domains rather than returning NaN.

## See also

- `docs/GLOBALS_RECIPE.md`
- `docs/ACTIVATIONS_RECIPE.md`
- `docs/DENSE_TRAINING_RECIPE.md`
