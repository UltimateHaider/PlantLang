# Normalization Recipe (v0.61.3)

Two normalization primitives with automatic backward through `GRAD`:

- **BatchNorm** — per-channel (4-D `[N,C,H,W]`) or per-feature (2-D `[N,F]`)
- **LayerNorm** — last axis, any ndim

Both are recorded as one tape node (`OP_BATCHNORM 19`, `OP_LAYERNORM 20`);
`GRAD` returns `dX`, `dγ` and `dβ`. ADv2 (`GRAD_HO`) raises an explicit
error instead of a wrong gradient.

## BatchNorm

### Forward — 4-D `[N,C,H,W]`, per-channel

```plant
CREATE X (TENSOR) TO TENSOR([[[[1.0, 2.0], [3.0, 4.0]]]]).   # [1,1,2,2]
CREATE g (TENSOR) TO TENSOR([1.0]).                          # [C]
CREATE b (TENSOR) TO TENSOR([0.0]).
CREATE y (TENSOR) TO BATCHNORM_FORWARD(X, g, b, 0.00001).
# y == [[[[-1.34164, -0.447212], [0.447212, 1.34164]]]]
```

### Forward — 2-D `[N,F]`, per-feature

```plant
CREATE X (TENSOR) TO TENSOR([[1.0, 10.0], [3.0, 30.0]]).     # [2,2]
CREATE g (TENSOR) TO TENSOR([1.0, 1.0]).
CREATE b (TENSOR) TO TENSOR([0.0, 0.0]).
CREATE y (TENSOR) TO BATCHNORM_FORWARD(X, g, b, 0.00001).
```

### Backward — `dX`, `dγ`, `dβ`

```plant
TENSOR_TRACK(X).
TENSOR_TRACK(g).
TENSOR_TRACK(b).
CREATE y    (TENSOR) TO BATCHNORM_FORWARD(X, g, b, 0.00001).
CREATE ym   (TENSOR) TO TENSOR_MUL(y, MASK).        # any downstream loss
CREATE loss (DBL)    TO TENSOR_SUM(ym).
CREATE dX (TENSOR) TO GRAD(loss, X, 1).
CREATE dg (TENSOR) TO GRAD(loss, g, 1).
CREATE db (TENSOR) TO GRAD(loss, b, 1).
TAPE_CLEAR().
```

> Tip: with an **affine** downstream gradient (`dy` proportional to
> `a + b·x̂`) `dX` is exactly zero — BatchNorm is invariant to per-channel
> shift/scale. Use a mask (as in `batchnorm_backward_x`) to exercise a
> non-trivial `dX`.

## LayerNorm

### Forward — any ndim, last axis

```plant
CREATE X (TENSOR) TO TENSOR([[1.0, 2.0, 3.0]]).       # [1,3]
CREATE g (TENSOR) TO TENSOR([1.0, 1.0, 1.0]).         # [F]
CREATE b (TENSOR) TO TENSOR([0.0, 0.0, 0.0]).
CREATE y (TENSOR) TO LAYERNORM_FORWARD(X, g, b, 0.00001).
# y == [[-1.22474, 0, 1.22474]]
```

### Backward

Same shape as BatchNorm: `GRAD(loss, X|g|b, 1)` → `dX`, `dγ`, `dβ`.

```plant
TENSOR_TRACK(X). TENSOR_TRACK(g). TENSOR_TRACK(b).
CREATE y  (TENSOR) TO LAYERNORM_FORWARD(X, g, b, 0.00001).
CREATE ym (TENSOR) TO TENSOR_MUL(y, MASK).
CREATE loss (DBL) TO TENSOR_SUM(ym).
CREATE dX (TENSOR) TO GRAD(loss, X, 1).
TAPE_CLEAR().
```

## Stacking

### CNN

```plant
CREATE y  (TENSOR) TO CONV2D_FORWARD(X, K, b_conv, 1, 0).
CREATE yn (TENSOR) TO BATCHNORM_FORWARD(y, g_bn, b_bn, 0.00001).
CREATE a  (TENSOR) TO TENSOR_RELU(yn).
CREATE f  (TENSOR) TO TENSOR_RESHAPE_N(a, 2, N, Co*Ho*Wo).
CREATE o  (TENSOR) TO DENSE_FORWARD(f, W, b_dense).
```

### Transformer block

```plant
CREATE h  (TENSOR) TO DENSE_FORWARD(X, W1, b1).
CREATE hn (TENSOR) TO LAYERNORM_FORWARD(h, g_ln, b_ln, 0.00001).
CREATE a  (TENSOR) TO TENSOR_GELU(hn).
```

## Limitations

- **No running statistics / momentum / train-eval flag** — BatchNorm always
  uses the statistics of the current batch (documented, v0.62.0+ candidate).
- BatchNorm: 2-D or 4-D only; LayerNorm: **last axis only**.
- `γ`/`β` must be `[C]` (BN) or `[F]` (LN).
- One `eps` value (double literal, e.g. `0.00001`).
- ADv2: `GRAD_HO` over either op errors with
  `ad2: BatchNorm/LayerNorm unsupported (use GRAD)`.
- No axis-reduction / `sqrt` tensor primitives yet (TD-068) — the ops use
  direct loops internally.

## Numerical notes

- Variance is **biased** (divide by `M`, not `M−1`).
- `eps > 0` guards `1/√(σ² + eps)`.
- Backward uses the compact stable form
  `dx = inv/M · (M·dhat − Σdhat − x̂·Σ(dhat·x̂))`, avoiding explicit
  `dμ`/`dσ²` (and their cancellation issues).
- Statistics are recomputed from `x` in backward with the same loop order
  as forward, so no state is stored on the tape (only `eps` in `scalar`).

## See also

- `docs/ACTIVATIONS_RECIPE.md`
- `docs/DENSE_TRAINING_RECIPE.md`
- `docs/CONV2D_RECIPE.md`
- `docs/NN_LIMITATIONS.md`
