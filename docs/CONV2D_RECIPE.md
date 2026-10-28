# CONV2D Recipe (v0.61.2)

`CONV2D_FORWARD(X, K, b, stride, pad)` — 2-D convolution over NCHW
tensors, recorded as a single taped op (`OP_CONV2D`) so `GRAD` returns
**dX**, **dK** and **db**.

## Overview

2-D convolution in **NCHW** with forward and backward (dX, dK, db) via the
old `GRAD` engine. ADv2 (`GRAD_HO`) is not supported — it raises an
explicit error instead of returning a wrong gradient.

## Shapes

| Tensor | Shape |
|---|---|
| `X` | `[N, C_in, H, W]` (4-D) |
| `K` | `[C_out, C_in, KH, KW]` (4-D) |
| `b` | `[C_out]` (1-D) or omitted/`NULL` |
| `out` | `[N, C_out, H_out, W_out]` |

`H_out = (H + 2·pad − KH) / stride + 1` (same for `W`), `stride ≥ 1`,
`pad ≥ 0`. A single `stride`/`pad` applies to both spatial dims.

## Forward

```plant
CREATE X (TENSOR) TO TENSOR([[[[1.0, 2.0], [3.0, 4.0]]]]).
CREATE K (TENSOR) TO TENSOR([[[[1.0, 0.0], [0.0, 1.0]]]]).
CREATE b (TENSOR) TO TENSOR([0.0]).
CREATE y (TENSOR) TO CONV2D_FORWARD(X, K, b, 1, 0).
SHOW y.                       # [[[[5]]]]  (2x2 identity kernel)
```

## Backward

```plant
TENSOR_TRACK(K).
CREATE y   (TENSOR) TO CONV2D_FORWARD(X, K, b, 1, 0).
CREATE y2  (TENSOR) TO TENSOR_MUL(y, y).
CREATE loss (DBL)   TO TENSOR_SUM(y2).
CREATE dK  (TENSOR) TO GRAD(loss, K, 1).
CREATE db  (TENSOR) TO GRAD(loss, b, 1).
TAPE_CLEAR().
```

`GRAD(loss, X)` gives `dX`. All three are computed by direct loops inside
the `OP_CONV2D` adjoint (no im2col temporaries), so memory stays flat.

## CNN pattern

```plant
CREATE y (TENSOR) TO CONV2D_FORWARD(X, K, b, 1, 0).   # [N,Co,Ho,Wo]
CREATE a (TENSOR) TO TENSOR_RELU(y).
CREATE f (TENSOR) TO TENSOR_RESHAPE_N(a, 2, N, Co*Ho*Wo).
CREATE o (TENSOR) TO DENSE_FORWARD(f, W, b2).
```

Use `TENSOR_RESHAPE_N(x, nd, d1, d2, …)` — the variadic form (TD-067) — it
does not allocate a list and cannot leak.

## Limitations

- **ADv2**: `GRAD_HO` over a CONV2D errors with
  `ad2: CONV2D unsupported (use GRAD)` — single-gradient-per-forward
  applies to the first-order tape.
- No dilation, no groups, no NCHW→NHWC (NCHW only), square-independent
  stride/pad per axis not supported (single value).
- No pooling ops (v0.61.3+ candidate).
- 4-D only; `matmul`/`transpose` remain 2-D.

## Stride / padding

`stride ≥ 1`, `pad ≥ 0` (one value covers both spatial axes).
`H_out = (H + 2·pad − KH) / stride + 1`, `W_out = (W + 2·pad − KW) / stride + 1`;
a kernel larger than the padded input is rejected
(`conv2d: kernel larger than input`).

## TD-067 fix (reshape)

The legacy `TENSOR_RESHAPE(a, [d1, d2, …])` now calls
`plant_tensor_reshape_and_free`, which frees the inline list after
reshaping (no leak). The new `TENSOR_RESHAPE_N(a, nd, d1, d2, …)` is
variadic and allocates no list at all:

```plant
CREATE r (TENSOR) TO TENSOR_RESHAPE_N(x, 2, 2, 2).   # [4] -> [2,2]
```

## Memory

Direct loops → no im2col buffer. Compute cost is
`N · C_out · C_in · KH · KW · H_out · W_out` multiply-adds; fine for
tests and small models, not tuned.

## See also

- `docs/ACTIVATIONS_RECIPE.md`
- `docs/DENSE_TRAINING_RECIPE.md`
- `docs/NN_LIMITATIONS.md`
