# Dropout Recipe (v0.61.4)

## Overview

Deterministic dropout with an explicit **seed** and a **local** PRNG
(xorshift64\*) — no global RNG state, fully reproducible. `p = 0` is eval
mode (identity). ADv2 (`GRAD_HO`) raises an explicit error.

## Forward

```plant
CREATE x (TENSOR) TO TENSOR([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]).
CREATE y (TENSOR) TO DROPOUT_FORWARD(x, 0.5, 1).     # p = 0.5, seed = 1
# y == [0, 0, 6, 0, 10, 12, 14, 0]   (dropped → 0, kept → x / (1-p))
```

Each element is kept with probability `1-p` and scaled by `1/(1-p)`, so the
expected value is preserved.

## Eval mode

```plant
CREATE y (TENSOR) TO DROPOUT_FORWARD(x, 0.0, 1).     # identity copy
```

## Backward

```plant
TENSOR_TRACK(x).
CREATE y    (TENSOR) TO DROPOUT_FORWARD(x, 0.5, 1).
CREATE y2   (TENSOR) TO TENSOR_MUL(y, y).
CREATE loss (DBL)    TO TENSOR_SUM(y2).
CREATE dx   (TENSOR) TO GRAD(loss, x, 1).
TAPE_CLEAR().
# dx == [0, 0, 24, 0, 40, 48, 56, 0]  (= dy * mask)
```

The mask is **not stored**: the backward rule replays the same PRNG stream
from `(p, seed)` in the same element order, so it always matches forward.

## Determinism

```plant
CREATE y1 (TENSOR) TO DROPOUT_FORWARD(x, 0.5, 1).
CREATE y2 (TENSOR) TO DROPOUT_FORWARD(x, 0.5, 1).    # y1 == y2
CREATE y3 (TENSOR) TO DROPOUT_FORWARD(x, 0.5, 2).    # y3 != y1
```

## Limitations

- `p` must satisfy `0 ≤ p < 1` (`p >= 1` → `dropout: p must be in [0,1)`).
- Seed is **explicit** — there is no global `srand`/RNG configuration.
- ADv2: `ad2: Dropout unsupported (use GRAD)`.
- No mask output / no "inverted dropout off" mode.
- Compiler does not use `DROPOUT_FORWARD` → bootstrap inert.

## See also

- `docs/NN_EXAMPLE.md`
- `docs/ACTIVATIONS_RECIPE.md`
- `docs/DENSE_TRAINING_RECIPE.md`
