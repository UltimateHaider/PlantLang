# NN Limitations (v0.61.0a)

Scope notes for the neural-network primitives shipped in v0.61.0a
(`DENSE_FORWARD`, `TENSOR_RAND_UNIFORM`) and their interaction with ADv2.

## DENSE layer

- **2-D only.** `DENSE_FORWARD(x, w, b)` = `matmul(x, w) + b`; `matmul` requires
  **2-D × 2-D** operands (`x: [batch, fan_in]`, `w: [fan_in, fan_out]`).
  Batched (>2-D) inputs and batched matmul are **out of scope** for now.
- **Bias broadcast:** `b: [fan_out]` broadcasts to `[batch, fan_out]` ✓
  (`_pt_ew` is broadcast-aware).
- **Weights:** 2-D `[fan_in, fan_out]`.
- **Init:** `TENSOR_RAND_UNIFORM([fan_in, fan_out], -r, r)` with
  `r = sqrt(6/(fan_in+fan_out))` (Glorot) or `r = sqrt(6/fan_in)` (He).

## ADv2 (higher-order)

| Order | Status |
|---|---|
| 1st | ✅ complete |
| 2nd | ✅ complete (includes **broadcast ADD**, TD-065 fixed in v0.61.0a) |
| 3rd+ | ❌ unsupported by design (explicit error gate) |

See `docs/ADV2_LIMITATIONS.md` for the full analysis.

## Intermediate ownership (TD-041 class)

An **anonymous** intermediate in a non-TENSOR target can leak:

```plant
# ❌ leaks the `y*y` tensor
CREATE loss (DBL) TO TENSOR_SUM(TENSOR_MUL(y, y)).

# ✅ name it, then free it
CREATE y2 (TENSOR) TO TENSOR_MUL(y, y).
CREATE loss (DBL) TO TENSOR_SUM(y2).
TENSOR_FREE(y2).
```

A hoist exists for TENSOR-typed targets (`CREATE w (TENSOR) TO <nested op>`),
but not for scalars (`DBL`) or for `TENSOR_SUM(<nested>)`. Root fix (hoisting
scalar targets) is deferred to v0.62.0+.

## RNG

- `plant_random()` (language `MATH_RANDOM`) exists: uniform `[0,1)`, backed by
  libc `rand()`.
- **No seed control yet.** Tests must use **literal weights** for determinism.
  Seeding → v0.62.0+ (Data Loading / Utilities).

## Matmul

- **2-D × 2-D only**; shape errors return explicit error tensors.
- Batched (3-D) matmul → v0.62.0+.

## Backward

- **1st order:** `GRAD(loss, w, 1)` with `retain` for several gradients
  (`dW`, `db`, `dX`) — works today.
- **2nd order:** `GRAD_HO` — works (softmax, broadcast, bias).
- **3rd order:** explicit error gate.

## `GRAD_HO` semantics reminder

`GRAD_HO` is **single-gradient-per-forward-pass**: a second `GRAD_HO` on the
same forward tape consumes the level-1 frame and yields a wrong value. Use one
`GRAD_HO` per forward pass (or re-run the forward).
