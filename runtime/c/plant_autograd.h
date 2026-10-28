/*
 * plant_autograd.h — v0.56.0a: reverse-mode Autograd (tape core).
 *
 * Records differentiable tensor ops on a global tape and runs a
 * reverse-mode backward pass to compute gradients.
 */

#ifndef PLANT_AUTOGRAD_H
#define PLANT_AUTOGRAD_H

#include "plant_tensor.h"

/* v0.56.0a — Reverse-mode Autograd (tape). */

/* Ops */
#define OP_ADD    1
#define OP_SUB    2
#define OP_MUL    3
#define OP_DIV    4
#define OP_SCALE  5
#define OP_SUM    6
#define OP_MEAN   7

/* v0.56.0b-1 — Matmul + core activations. */
#define OP_MATMUL    8
#define OP_RELU      9
#define OP_SIGMOID   10
#define OP_TANH      11
#define OP_TRANSPOSE 12
#define OP_RESHAPE   13

/* v0.56.0c — Modern activations. */
#define OP_SOFTMAX   14
#define OP_GELU      15
#define OP_SWISH     16
#define OP_MISH      17

/* v0.61.2 — CONV2D. */
#define OP_CONV2D   18

/* v0.61.3 — normalisation layers. */
#define OP_BATCHNORM 19
#define OP_LAYERNORM 20

/* v0.61.4 — dropout. */
#define OP_DROPOUT 21

/* Init: register the tape hook with plant_tensor.c. */
void plant_autograd_init(void);

/* Tape management (called internally by hooked ops). */
void plant_tape_record(int op, PlantTensor* a, PlantTensor* b,
                       double s, PlantTensor* out);

/* v0.61.2 — CONV2D recorder (x, k, optional bias, stride, pad). */
void plant_tape_record_conv(PlantTensor* x, PlantTensor* k, PlantTensor* b,
                            int64_t stride, int64_t pad, PlantTensor* out);

/* v0.61.3 — norm recorders (x, gamma, beta, eps). */
void plant_tape_record_batchnorm(PlantTensor* x, PlantTensor* g, PlantTensor* b,
                                 double eps, PlantTensor* out);
void plant_tape_record_layernorm(PlantTensor* x, PlantTensor* g, PlantTensor* b,
                                 double eps, PlantTensor* out);

/* v0.61.4 — dropout recorder (p, seed). */
void plant_tape_record_dropout(PlantTensor* x, double p, int64_t seed,
                               PlantTensor* out);
void plant_tape_clear(void);

/* Core GRAD (4-arg + v0.57.3 retain flag). */
PlantTensor* plant_autograd_grad(PlantTensor* loss_t, double loss_d,
                                 PlantTensor* w, int use_tensor, int retain);

/* Convenience GRAD (2-arg; mapper target). */
PlantTensor* plant_grad(double loss_d, PlantTensor* w);

/* v0.57.3 — 3-arg GRAD with explicit retain_graph flag. */
PlantTensor* plant_grad_retain(double loss_d, PlantTensor* w, int retain);
/* v0.57.3 — TAPE_CLEAR(): explicit tape cleanup (idempotent). */
void plant_tape_clear_public(void);

#endif /* PLANT_AUTOGRAD_H */
