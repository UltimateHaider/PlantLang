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

/* Init: register the tape hook with plant_tensor.c. */
void plant_autograd_init(void);

/* Tape management (called internally by hooked ops). */
void plant_tape_record(int op, PlantTensor* a, PlantTensor* b,
                       double s, PlantTensor* out);
void plant_tape_clear(void);

/* Core GRAD (4-arg). */
PlantTensor* plant_autograd_grad(PlantTensor* loss_t, double loss_d,
                                 PlantTensor* w, int use_tensor);

/* Convenience GRAD (2-arg; mapper target). */
PlantTensor* plant_grad(double loss_d, PlantTensor* w);

#endif /* PLANT_AUTOGRAD_H */
