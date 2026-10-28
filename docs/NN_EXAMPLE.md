# Full NN Example (v0.61.4)

End-to-end training step: **forward → backward → SGD update**, using only
the primitives shipped in v0.61.0a–v0.61.4.

## MLP (DENSE → BN → ReLU → Dropout → DENSE → Softmax → MSE)

```plant
# 1. Data + parameters
CREATE X (TENSOR) TO TENSOR([[1.0, 2.0], [3.0, 4.0]]).
CREATE W1 (TENSOR) TO TENSOR([[0.5], [0.5]]).
CREATE b1 (TENSOR) TO TENSOR([0.0]).
CREATE gamma (TENSOR) TO TENSOR([1.0]).
CREATE beta (TENSOR) TO TENSOR([0.0]).
CREATE W2 (TENSOR) TO TENSOR([[0.5, 0.2]]).
CREATE b2 (TENSOR) TO TENSOR([0.0, 0.0]).
CREATE T (TENSOR) TO TENSOR([[0.0, 1.0], [0.0, 1.0]]).

# 2. Track everything that receives a gradient
TENSOR_TRACK(W1).
TENSOR_TRACK(W2).

# 3. Forward
CREATE h1  (TENSOR) TO DENSE_FORWARD(X, W1, b1).
CREATE h1n (TENSOR) TO BATCHNORM_FORWARD(h1, gamma, beta, 0.00001).
CREATE a1  (TENSOR) TO TENSOR_RELU(h1n).
CREATE a1d (TENSOR) TO DROPOUT_FORWARD(a1, 0.0, 1).      # 0 = eval/train-off
CREATE out (TENSOR) TO DENSE_FORWARD(a1d, W2, b2).
CREATE p   (TENSOR) TO TENSOR_SOFTMAX(out, 1).

# 4. Loss — MSE on softmax (no log primitive yet: TD-069)
CREATE diff (TENSOR) TO TENSOR_SUB(p, T).
CREATE sq   (TENSOR) TO TENSOR_MUL(diff, diff).
CREATE loss (DBL)    TO TENSOR_SUM(sq).

# 5. Backward (retain = 1 → several gradients from one forward)
CREATE dW1 (TENSOR) TO GRAD(loss, W1, 1).
CREATE dW2 (TENSOR) TO GRAD(loss, W2, 1).

# 6. TAPE_CLEAR *before* touching the tensors that the tape still holds
TAPE_CLEAR().

# 7. SGD step (expression form: SET frees the old value, 0 leaks)
CREATE lr (DBL) TO 0.5.
CREATE s1 (TENSOR) TO TENSOR_SCALE(dW1, lr).
CREATE s2 (TENSOR) TO TENSOR_SCALE(dW2, lr).
SET W1 TO TENSOR_SUB(W1, s1).
SET W2 TO TENSOR_SUB(W2, s2).

# 8. Re-measure the loss (new forward → new tape)
CREATE h1b  (TENSOR) TO DENSE_FORWARD(X, W1, b1).
CREATE h1nb (TENSOR) TO BATCHNORM_FORWARD(h1b, gamma, beta, 0.00001).
CREATE a1b  (TENSOR) TO TENSOR_RELU(h1nb).
CREATE outb (TENSOR) TO DENSE_FORWARD(a1b, W2, b2).
CREATE pb   (TENSOR) TO TENSOR_SOFTMAX(outb, 1).
CREATE db_  (TENSOR) TO TENSOR_SUB(pb, T).
CREATE sqb  (TENSOR) TO TENSOR_MUL(db_, db_).
CREATE loss2 (DBL)   TO TENSOR_SUM(sqb).
TAPE_CLEAR()

# 9. Free (see tests/native/nn_end_to_end.plant for the exact list)
```

Measured with the snippet above (`lr = 0.5`): `loss = 1.159967568` →
`loss2 = 0.8783538118` after one step (0 B leaks). With `lr = 5.0`
(`tests/native/nn_end_to_end.plant`, regression-locked):
`1.159967568 → 0.5000476938`.

## CNN (CONV2D → BN → ReLU → flatten → DENSE)

```plant
CREATE X (TENSOR) TO TENSOR([[[[1.0, 2.0], [3.0, 4.0]]]]).
CREATE K (TENSOR) TO TENSOR([[[[1.0]]]]).
CREATE b_conv (TENSOR) TO TENSOR([0.0]).
CREATE gamma (TENSOR) TO TENSOR([1.0]).
CREATE beta (TENSOR) TO TENSOR([0.0]).
CREATE y  (TENSOR) TO CONV2D_FORWARD(X, K, b_conv, 1, 0).
CREATE yn (TENSOR) TO BATCHNORM_FORWARD(y, gamma, beta, 0.00001).
CREATE a  (TENSOR) TO TENSOR_RELU(yn).
CREATE af (TENSOR) TO TENSOR_RESHAPE_N(a, 2, 1, 4).
CREATE W  (TENSOR) TO TENSOR([[0.5], [0.5], [0.5], [0.5]]).
CREATE b  (TENSOR) TO TENSOR([0.0]).
CREATE out (TENSOR) TO DENSE_FORWARD(af, W, b).
```

Output: `[[0.894424]]` (regression-locked by `nn_cnn_forward`).

## Rules of thumb

1. **Name intermediates** — nested tensor expressions leak (TD-041 class).
2. **`TAPE_CLEAR()` after the backward** and, critically, **before** you
   `SET`/mutate tensors the tape still holds (TD-070).
3. Use **`retain = 1`** for multiple gradients from one forward (TD-066).
4. Prefer **`SET W TO TENSOR_SUB(W, step)`** (expression form). The
   variable form `SET W TO Wn` *retains* `Wn`, leaving an extra reference
   you would have to release.
5. Literal weights in tests (no weight-init seeding yet); `TENSOR_RAND_UNIFORM`
   exists but is not seeded.
6. Loss: **MSE on softmax** until `log` exists (TD-069).

## See also

- `docs/DROPOUT_RECIPE.md`
- `docs/NORM_RECIPE.md`
- `docs/CONV2D_RECIPE.md`
- `docs/DENSE_TRAINING_RECIPE.md`
- `docs/NN_LIMITATIONS.md`
