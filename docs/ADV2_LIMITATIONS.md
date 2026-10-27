# ADv2 Limitations — Higher-Order AD

## Summary

ADv2 is the reverse-mode automatic differentiation engine that powers
`GRAD_HO(loss, w)` in PlantLang. This document specifies its supported
scope, its design limitations, and the engineering path for future
extensions.

## Supported Scope

| Order | Status | Use cases |
|---|---|---|
| 1st derivative | ✅ complete | Gradient descent, SGD, Adam, most training |
| 2nd derivative | ✅ complete | Newton's method, PINNs, MAML, Hessians |
| 3rd+ derivative | ❌ unsupported by design | (no mainstream use case) |

### 1st Order (complete)

- Forward pass taped (17 forward hooks).
- Backward pass taped (every supported op has a double-backward rule).
- `GRAD_HO(loss, w)` returns `dL/dw`.
- Validated: `x² → 2x`, `x³ → 3x²`, softmax Jacobian.

### 2nd Order (complete)

- `GRAD_HO(GRAD_HO(loss, w), w)` returns `d²L/dw²`.
- Validated: `x² → 2`, `x³ → 6x`, softmax 2nd → 0.
- **Strong validation:** softmax non-trivial `[-0.25, 0.25]` matches the
  old first-order engine's published reference
  (`gradient_softmax_nontrivial`) exactly.

### 3rd+ Order (unsupported by design)

- Explicit error:
  `ad2: 3rd+ derivatives unsupported (single-DAG pending 0c)`.
- **No silent-wrong answer.**
- No plan to support it (see below).

## Why 3rd+ Order is Unsupported

### Use-case analysis (ML industry)

| Technique | Order | Notes |
|---|---|---|
| SGD, Adam, AdamW, RMSprop | 1st | All standard optimizers |
| Momentum, Nesterov | 1st | Variants of SGD |
| Newton's method | 2nd | Uses the Hessian |
| Gauss-Newton | 2nd | Approximate Hessian |
| Levenberg-Marquardt | 2nd | Damped Gauss-Newton |
| L-BFGS | 2nd | Limited-memory BFGS |
| PINNs | 2nd | PDEs are at most 2nd order |
| MAML | 2nd | Gradient of gradient |
| Hessian-vector products | 2nd | Hutchinson's estimator |
| Natural gradient | 2nd | Fisher information |
| 3rd-order optimizers | 3rd | none in production |
| Higher-order PDEs | 3rd+ | extremely rare (KdV, Boussinesq — not DL) |

**Conclusion:** 2nd order covers all mainstream ML. 3rd+ order is an
academic niche.

### Engineering cost analysis

| Path | LOC | Time | Risk |
|---|---|---|---|
| Current ADv2 (0a+0b+0c) | 581 lines | done | ✅ zero |
| ADv2 v2 (3rd order) | +200–300 lines | 1–2 weeks | 🔴 HIGH |

The cost/benefit ratio is negative: no real use case justifies the
engineering cost or the risk to the green baseline.

## What was tried (v0.60.0d investigation)

Three approaches were attempted to enable 3rd order via small patches.
All were **disproven**:

1. **B1 — identity edge per final accumulator:** over-counts
   (`x³` 3rd = `[21, 109]` instead of `[6, 6]`).
2. **B2 — root the chain at the target:** runaway
   (`x³` 3rd = `[90, 942]`).
3. **C+B atomic (flatten + processed guard + rooted):** invalid by
   design —
   - C (flatten + guard) is a **no-op**: every node already belongs to
     one frame and each frame is traversed once per pass;
   - B (identity edge) has **no correct placement** (B1 over-counts,
     B2 re-injects);
   - the per-tensor accumulator model cannot express node-keyed gradient
     graphs.

### Root cause (structural)

The "per-tensor accumulator + frame segments" model of ADv2 cannot
express **node-keyed gradient graphs**, which is what higher-order AD
requires. This is a structural limit, not an implementation bug.

## Correct path (ADv2 v2)

The PyTorch model:

- Gradients live on **forward nodes** (not on anonymous accumulators).
- Each forward node gets a gradient expression built from its inputs;
  the node itself is the key.
- The backward pass applies each node's rule **at most once per level**
  and immediately continues through that node's inputs.
- Traversal is a single reverse-topological walk over the one forward
  graph, not a stack of per-level tapes.

- Effort: ~200–300 lines (redesign of the tape/backward core).
- Trigger: only if a real use case for 3rd order emerges.

## Semantics (documented)

`GRAD_HO` is **single-gradient-per-forward-pass**:

- A second `GRAD_HO` on the same forward tape consumes the level-1 frame
  and yields a wrong value.
- Use one `GRAD_HO` per forward pass (or re-run the forward).
- Differs from the old `GRAD` (which re-uses/clears its tape).

Guards:

- `DIV`: `b == 0` → explicit error.
- `MISH`: `x == 0` → documented edge (uses `out/x`).
- `SOFTMAX`: `ndim` 1 or 2 only → explicit error otherwise.
- `MUL`/`DIV` broadcast: same-shape and 2-D × 1-D supported; other
  broadcast shapes → explicit error.

## Test Coverage

- **1st order:** 20+ tests (`x²`, `x³`, `sum`, activations, matmul,
  softmax 1-D/2-D, broadcast).
- **2nd order:** 15+ tests (`x²`, `x³`, matmul, softmax, broadcast).
- **3rd-order gate:** 1 test (`td048_adv2_third_unsupported` → explicit
  error).
- **Regression:** the old `GRAD` still works
  (`td048_adv2_grad_regression`).

All tests are leak-free (0 B definitely lost).

## Validation

- Softmax Jacobian validated against the old engine's published
  reference: `gradient_softmax_nontrivial` → `[-0.25, 0.25]` (exact).
- 2nd-order correctness verified for `x²`, `x³`, matmul, softmax.
- 1st/2nd-order behaviour unchanged from the v0.58.12 baseline.

## Future

- ADv2 v2 (node-keyed gradients) = **v0.61.x+** epic.
- Design doc + `/tmp` prototype first (as done for the diagnosis).
- 292-test baseline as guard.
- Only if a real use case for 3rd order emerges.

## Related Documents

- `docs/TECH_DEBT.md` — TD-048 CLOSED (re-scoped).
- `docs/CHANGELOG.md` — v0.60.0a/b/c/d entries.
- `docs/RELEASES.md` — release timeline.
