# DENSE Training Recipe (v0.61.0a)

A complete, leak-free recipe for a fully-connected layer in PlantLang.

## 1. Forward

```plant
# x: [batch, fan_in]   w: [fan_in, fan_out]   b: [fan_out]
CREATE X (TENSOR) TO TENSOR([[1.0, 2.0], [3.0, 4.0]]).
CREATE W (TENSOR) TO TENSOR([[0.5, 0.5], [0.5, 0.5]]).
CREATE b (TENSOR) TO TENSOR([0.1, 0.2]).
CREATE y (TENSOR) TO DENSE_FORWARD(X, W, b).       # X·W + b (broadcast)
```

Random initialisation (Glorot):

```plant
CREATE W (TENSOR) TO TENSOR_RAND_UNIFORM([2, 2], -0.866, 0.866).
CREATE b (TENSOR) TO TENSOR_RAND_UNIFORM([2], -0.866, 0.866).
```

## 2. Track the parameters

```plant
TENSOR_TRACK(W).
TENSOR_TRACK(b).
```

## 3. Loss — **name intermediates**

```plant
# ❌ leaks the y*y tensor (TD-041 class)
CREATE loss (DBL) TO TENSOR_SUM(TENSOR_MUL(y, y)).

# ✅ safe
CREATE d (TENSOR) TO TENSOR_SUB(y, target).   # residual
CREATE d2 (TENSOR) TO TENSOR_MUL(d, d).      # squared residual
CREATE loss (DBL) TO TENSOR_SUM(d2).         # MSE (sum form)
TENSOR_FREE(d2).
TENSOR_FREE(d).
```

## 4. Backward (1st order) — one `GRAD` per parameter, `retain`

```plant
CREATE dW (TENSOR) TO GRAD(loss, W, 1).
CREATE db (TENSOR) TO GRAD(loss, b, 1).
TAPE_CLEAR().
```

`retain = 1` keeps the tape alive for the second `GRAD`; repeated first-order
gradients are supported (regression-locked by `grad_*` tests).

For a second-order pass use `GRAD_HO` (one gradient per forward):

```plant
CREATE dW  (TENSOR) TO GRAD_HO(loss, W).
CREATE ddW (TENSOR) TO GRAD_HO(TENSOR_SUM(dW), W).
```

## 5. SGD step

```plant
CREATE step (TENSOR) TO TENSOR_SCALE(dW, 0.01).
CREATE Wnew (TENSOR) TO TENSOR_SUB(W, step).
TENSOR_FREE(step).
TENSOR_FREE(W).
SET W TO Wnew.          # SET frees the old W (v0.58.0) — or assign via CREATE
```

(`SET W TO <bare tensor var>` retains on alias — v0.58.1 — so aliasing is safe.)

## 6. Teardown

```plant
TENSOR_FREE(W).
TENSOR_FREE(b).
TENSOR_FREE(y).
TENSOR_FREE(X).
```

## Rules

1. **Name intermediates** before `TENSOR_SUM`/`TENSOR_MEAN` (TD-041 class).
2. **`TAPE_CLEAR()`** after a 1st-order backward; **one `GRAD_HO` per forward**
   for higher order.
3. **Literal weights in tests** (no RNG seeding yet).
4. **2-D only** — batched inputs are future work (see `NN_LIMITATIONS.md`).
5. Free every tensor you create (`TENSOR_FREE`), including grad results.
