# Activations Recipe (v0.61.1)

All seven activations are runtime tensor ops with codegen mappers and
backward support in **both** engines (old `GRAD` and `GRAD_HO`).

## Activation table

| Op | Mapper | Formula | 1st derivative | 2nd derivative (a.e.) |
|---|---|---|---|---|
| ReLU | `TENSOR_RELU(x)` | `max(0, x)` | `1 if x>0 else 0` | `0` |
| Sigmoid | `TENSOR_SIGMOID(x)` | `1/(1+e^-x)` | `s(1-s)` | `s(1-s)(1-2s)` |
| Tanh | `TENSOR_TANH(x)` | `(e^x-e^-x)/(e^x+e^-x)` | `1-t²` | `-2t(1-t²)` |
| GELU | `TENSOR_GELU(x)` | `x·Φ(x)` (tanh approximation) | `Φ(x) + x·φ(x)` | (taped) |
| Swish | `TENSOR_SWISH(x)` | `x·σ(x)` | `σ + x·σ(1-σ)` | (taped) |
| Mish | `TENSOR_MISH(x)` | `x·tanh(softplus(x))` | `t + x(1-t²)σ` | (taped) |
| Softmax | `TENSOR_SOFTMAX(x, axis)` | `e^xᵢ/Σⱼe^xⱼ` | `out⊙(d − Σ out⊙d)` | (taped) |

Reference values (input `[-1, 0, 1]`): ReLU `[0, 0, 1]`, sigmoid
`[0.269, 0.5, 0.731]`, tanh `[-0.762, 0, 0.762]`, GELU
`[-0.159, 0, 0.841]`, swish `[-0.269, 0, 0.731]`, mish
`[-0.303, 0, 0.865]`, softmax `[0.090, 0.245, 0.665]`.

## Softmax axis

- **1-D** → `axis = 0`.
- **2-D** → `axis = 0` (columns) or `axis = 1` (rows).
- **3-D+** → explicit error (`softmax: only 1d/2d`).

## DENSE + activation pattern

```plant
CREATE h (TENSOR) TO DENSE_FORWARD(X, W, b).
CREATE a (TENSOR) TO TENSOR_RELU(h).
```

## Backward (first order) — **one gradient per forward**

```plant
TENSOR_TRACK(W).
CREATE a2 (TENSOR) TO TENSOR_MUL(a, a).        # name intermediates!
CREATE loss (DBL) TO TENSOR_SUM(a2).
CREATE dW (TENSOR) TO GRAD(loss, W).           # 2-arg: clears the tape
TENSOR_FREE(a2).
```

> ⚠️ **Do not use `retain = 1` with a DBL loss** — `GRAD(loss, w, 1)` currently
> fails with `grad: loss does not match retained tape` whenever the loss value
> is `%g`-rounded (see **TD-066**). Instead, recompute the forward pass for each
> gradient you need:

```plant
CREATE dW2 (TENSOR) TO GRAD(loss, W2).         # pass 1
TAPE_CLEAR().
# … rebuild h1/a1/h2/loss with the same inputs …
CREATE dW1 (TENSOR) TO GRAD(loss2, W1).        # pass 2
```

## Second order (`GRAD_HO`)

```plant
CREATE db  (TENSOR) TO GRAD_HO(loss, b).
CREATE d2b (TENSOR) TO GRAD_HO(TENSOR_SUM(db), b).
```

`GRAD_HO` is **single-gradient-per-forward-pass** (one gradient per forward).
Bias broadcast is fully supported (TD-065, v0.61.0a), e.g.
`d²(Σσ²)/db²` through a DENSE bias works.

## Common mistakes

1. **Anonymous intermediates** (TD-041 class) — `TENSOR_SUM(TENSOR_MUL(a, a))`
   leaks. Name and free:
   ```plant
   CREATE a2 (TENSOR) TO TENSOR_MUL(a, a).
   CREATE loss (DBL) TO TENSOR_SUM(a2).
   TENSOR_FREE(a2).
   ```
2. **Forgetting `TAPE_CLEAR()`** after a retained backward → tape leak.
3. **Softmax axis mismatch** (1-D vs 2-D).
4. **Multiple gradients from one forward** via `retain` — see TD-066; use one
   forward per gradient.

## See also

- `docs/DENSE_TRAINING_RECIPE.md`
- `docs/NN_LIMITATIONS.md`
- `docs/ADV2_LIMITATIONS.md`
